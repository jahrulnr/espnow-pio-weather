#include "slave.h"

#include "payload_codec.h"
#include "state_binary.h"
#include "weather_pipeline.h"

#include "app/weather/open_meteo_locations.h"

#include <app_config.h>
#include <WiFi.h>
#include <cstring>
#include <esp_log.h>
#include <esp_wifi.h>

namespace app::espnow {

namespace {

class SlaveStateSink final : public IStateSink {
 public:
  void injectNode(SlaveNode* slaveNode) {
    node = slaveNode;
  }

  bool publishState(const String& payload) override {
    if (node == nullptr) {
      return false;
    }

    String stateName;
    if (!app::espnow::codec::getField(payload, "state", stateName) || stateName != "weather") {
      return false;
    }

    String ok;
    String code;
    String time;
    String temperature;
    String windspeed;
    String winddirection;

    app::espnow::codec::getField(payload, "ok", ok);
    app::espnow::codec::getField(payload, "code", code);
    app::espnow::codec::getField(payload, "time", time);
    app::espnow::codec::getField(payload, "temperature", temperature);
    app::espnow::codec::getField(payload, "windspeed", windspeed);
    app::espnow::codec::getField(payload, "winddirection", winddirection);

    if (time.isEmpty() || temperature.isEmpty() || windspeed.isEmpty() || winddirection.isEmpty()) {
      ESP_LOGW("WEATHER", "Skip weather send: incomplete parsed fields");
      return false;
    }

    app::espnow::state_binary::WeatherState state = {};
    app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Weather);
    state.ok = static_cast<uint8_t>(ok == "1" || ok == "true" || ok == "ok");
    state.code = static_cast<int16_t>(code.toInt());
    strncpy(state.time, time.c_str(), sizeof(state.time) - 1);
    state.temperature10 = static_cast<int16_t>(temperature.toFloat() * 10.0f);
    state.windspeed10 = static_cast<int16_t>(windspeed.toFloat() * 10.0f);
    state.winddirection = static_cast<uint16_t>(winddirection.toInt());

    return node->sendStateBinary(&state, sizeof(state));
  }

 private:
  SlaveNode* node = nullptr;
};

SlaveStateSink stateSink;
WeatherCommandPipeline weatherPipeline;

bool sendIdentityStateNow(SlaveNode& node) {
  app::espnow::state_binary::IdentityState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
  strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
  state.id[sizeof(state.id) - 1] = '\0';

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("WEATHER", "Failed sending identity before weather request");
  }
  return sent;
}

bool sendFeaturesStateNow(SlaveNode& node) {
  app::espnow::state_binary::FeaturesState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
  state.contractVersion = 1;
  state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureSensor)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureWeather)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureProxyClient);

  const bool sent = node.sendStateBinary(&state, sizeof(state));
  if (!sent) {
    ESP_LOGW("WEATHER", "Failed sending features before weather request");
  }
  return sent;
}

bool sendWeatherProxyRequestNow(SlaveNode& node) {
  sendIdentityStateNow(node);
  sendFeaturesStateNow(node);

  const app::weather::Area area = static_cast<app::weather::Area>(WEATHER_AREA_INDEX);
  const String weatherUrl = app::weather::buildCurrentWeatherUrl(area);
  if (weatherUrl.isEmpty()) {
    ESP_LOGW("WEATHER", "Cannot build weather URL for sync request");
    return false;
  }

  app::espnow::state_binary::ProxyReqState request = {};
  app::espnow::state_binary::initHeader(request.header, app::espnow::state_binary::Type::ProxyReq);
  request.method = static_cast<uint8_t>(app::espnow::state_binary::HttpMethod::Get);
  strncpy(request.url, weatherUrl.c_str(), sizeof(request.url) - 1);
  request.url[sizeof(request.url) - 1] = '\0';

  const bool sent = node.sendStateBinary(&request, sizeof(request));
  if (sent) {
    ESP_LOGI("WEATHER", "Triggered weather proxy request by master command");
  } else {
    ESP_LOGW("WEATHER", "Failed sending weather proxy request to master");
  }
  return sent;
}

}  // namespace

static const char* TAG = "espnow_slave";
static constexpr uint8_t MIN_SCAN_CHANNEL = 1;
static constexpr uint8_t MAX_SCAN_CHANNEL = 13;
static constexpr uint32_t CHANNEL_SCAN_INTERVAL_MS = 300;
static constexpr uint32_t MASTER_TIMEOUT_MS = 12000;

SlaveNode* SlaveNode::activeInstance = nullptr;
SlaveNode espnowSlave;

bool SlaveNode::begin(uint8_t channel) {
  if (started) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (channel > 0) {
    esp_err_t channelErr = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set WiFi channel %d: %s", channel, esp_err_to_name(channelErr));
    }
    scanChannel = channel;
  } else {
    scanChannel = MIN_SCAN_CHANNEL;
  }

  esp_err_t initErr = esp_now_init();
  if (initErr != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(initErr));
    return false;
  }

  activeInstance = this;
  esp_now_register_send_cb(SlaveNode::onSendStatic);
  esp_now_register_recv_cb(SlaveNode::onReceiveStatic);

  started = true;
  lastHelloMs = millis();
  lastScanMs = millis();
  lastMasterSeenMs = 0;
  stateSink.injectNode(this);
  weatherPipeline.injectStateSink(&stateSink);
  if (!weatherPipeline.begin()) {
    ESP_LOGW(TAG, "Weather pipeline task failed to start");
  }
  ESP_LOGI(TAG, "ESP-NOW slave ready");
  return true;
}

void SlaveNode::loop() {
  if (!started) {
    return;
  }

  const uint32_t now = millis();
  if (masterKnown && lastMasterSeenMs > 0 && (now - lastMasterSeenMs > MASTER_TIMEOUT_MS)) {
    masterKnown = false;
    memset(masterMac, 0, sizeof(masterMac));
    ESP_LOGW(TAG, "Master beacon timeout, returning to channel scan");
  }

  if (!masterKnown && (now - lastScanMs >= CHANNEL_SCAN_INTERVAL_MS)) {
    scanNextChannel();
    lastScanMs = now;
  }

  if (masterKnown && (now - lastHelloMs >= 7000)) {
    static const char hello[] = "slave-online";
    sendToMaster(PacketType::HELLO, hello, sizeof(hello) - 1);
    lastHelloMs = now;
  }
}

bool SlaveNode::matchesMasterBeacon(const uint8_t* payload, uint8_t payloadSize) const {
  if (payload == nullptr || payloadSize != MASTER_BEACON_ID_LEN) {
    return false;
  }

  return memcmp(payload, MASTER_BEACON_ID, MASTER_BEACON_ID_LEN) == 0;
}

void SlaveNode::scanNextChannel() {
  uint8_t nextChannel = scanChannel;
  if (nextChannel < MIN_SCAN_CHANNEL || nextChannel >= MAX_SCAN_CHANNEL) {
    nextChannel = MIN_SCAN_CHANNEL;
  } else {
    nextChannel++;
  }

  const esp_err_t err = esp_wifi_set_channel(nextChannel, WIFI_SECOND_CHAN_NONE);
  if (err == ESP_OK) {
    scanChannel = nextChannel;
    ESP_LOGD(TAG, "Scanning channel %u", scanChannel);
    return;
  }

  ESP_LOGW(TAG, "Failed switching to channel %u: %s", nextChannel, esp_err_to_name(err));
}

bool SlaveNode::addMasterPeer(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }

  if (esp_now_is_peer_exist(mac)) {
    memcpy(masterMac, mac, 6);
    masterKnown = true;
    return true;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.ifidx = WIFI_IF_STA;
  peer.channel = 0;
  peer.encrypt = false;

  esp_err_t addErr = esp_now_add_peer(&peer);
  if (addErr != ESP_OK) {
    ESP_LOGW(TAG, "Failed add master peer: %s", esp_err_to_name(addErr));
    return false;
  }

  memcpy(masterMac, mac, 6);
  masterKnown = true;
  ESP_LOGI(TAG, "Master registered: %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

bool SlaveNode::sendToMaster(PacketType type, const void* payload, size_t payloadSize) {
  if (!started || !masterKnown) {
    return false;
  }

  Frame frame = {};
  frame.header.version = PROTOCOL_VERSION;
  frame.header.type = static_cast<uint8_t>(type);
  frame.header.sequence = sequence++;
  frame.header.timestampMs = millis();

  frame.payloadSize = payloadSize > MAX_PAYLOAD_SIZE ? MAX_PAYLOAD_SIZE : payloadSize;
  if (frame.payloadSize > 0 && payload != nullptr) {
    memcpy(frame.payload, payload, frame.payloadSize);
  }

  const size_t bytes = sizeof(frame.header) + sizeof(frame.payloadSize) + frame.payloadSize;
  esp_err_t sendErr = esp_now_send(masterMac, reinterpret_cast<const uint8_t*>(&frame), bytes);
  if (sendErr != ESP_OK) {
    ESP_LOGW(TAG, "Send to master failed: %s", esp_err_to_name(sendErr));
    return false;
  }

  return true;
}

bool SlaveNode::sendState(const char* text) {
  if (text == nullptr) {
    return false;
  }

  return sendToMaster(PacketType::STATE, text, strlen(text));
}

bool SlaveNode::sendStateBinary(const void* payload, size_t payloadSize) {
  if (payload == nullptr || payloadSize == 0) {
    return false;
  }

  return sendToMaster(PacketType::STATE, payload, payloadSize);
}

void SlaveNode::onSendStatic(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
  if (!activeInstance) {
    return;
  }

  if (tx_info == nullptr) {
    ESP_LOGD(TAG, "TX done -> %s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
    return;
  }

  ESP_LOGD(TAG, "TX status=%s", status == ESP_NOW_SEND_SUCCESS ? "ok" : "fail");
}

void SlaveNode::onReceiveStatic(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
  if (!activeInstance || recv_info == nullptr || data == nullptr || len <= 0) {
    return;
  }

  if (len < static_cast<int>(sizeof(PacketHeader) + sizeof(uint8_t))) {
    ESP_LOGW(TAG, "Received frame too small: %d", len);
    return;
  }

  const auto* header = reinterpret_cast<const PacketHeader*>(data);
  const auto payloadSize = *(data + sizeof(PacketHeader));
  const auto* payload = data + sizeof(PacketHeader) + sizeof(uint8_t);
  const size_t expectedLen = sizeof(PacketHeader) + sizeof(uint8_t) + payloadSize;
  if (payloadSize > MAX_PAYLOAD_SIZE || expectedLen > static_cast<size_t>(len)) {
    ESP_LOGW(TAG, "Invalid frame size: payload=%u len=%d", payloadSize, len);
    return;
  }

  const auto type = static_cast<PacketType>(header->type);
  const bool fromKnownMaster = activeInstance->masterKnown && (memcmp(activeInstance->masterMac, recv_info->src_addr, 6) == 0);
  const bool validBeacon = activeInstance->matchesMasterBeacon(payload, payloadSize);

  if (!fromKnownMaster) {
    if ((type != PacketType::HELLO && type != PacketType::HEARTBEAT) || !validBeacon) {
      ESP_LOGD(TAG, "Ignoring packet from unknown sender");
      return;
    }

    if (!activeInstance->addMasterPeer(recv_info->src_addr)) {
      return;
    }
    ESP_LOGI(TAG, "Master beacon matched, locked to master");
  }

  if ((type == PacketType::HELLO || type == PacketType::HEARTBEAT) && validBeacon) {
    activeInstance->lastMasterSeenMs = millis();
    activeInstance->scanChannel = WiFi.channel();
  }

  switch (type) {
    case PacketType::HELLO: {
      static const char hello[] = "slave-online";
      activeInstance->sendToMaster(PacketType::HELLO, hello, sizeof(hello) - 1);
      break;
    }
    case PacketType::HEARTBEAT: {
      app::espnow::state_binary::SlaveAliveState state = {};
      app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::SlaveAlive);
      activeInstance->sendToMaster(PacketType::STATE, &state, sizeof(state));
      break;
    }
    case PacketType::COMMAND:
      if (payloadSize > 0) {
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::IdentityReq,
                                                      sizeof(app::espnow::state_binary::IdentityReqCommand))) {
          sendIdentityStateNow(*activeInstance);
          sendFeaturesStateNow(*activeInstance);
          break;
        }

        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::WeatherSyncReq,
                                                      sizeof(app::espnow::state_binary::WeatherSyncReqCommand))) {
          sendWeatherProxyRequestNow(*activeInstance);
          break;
        }

        if (!weatherPipeline.submitCommand(payload, payloadSize)) {
          ESP_LOGW(TAG, "Failed queueing command payload");
        }
      } else {
        ESP_LOGI(TAG, "Command packet received (empty)");
      }
      break;
    case PacketType::STATE:
      if (payloadSize > 0) {
        if (app::espnow::state_binary::hasTypeAndSize(payload,
                                                      payloadSize,
                                                      app::espnow::state_binary::Type::MasterNet,
                                                      sizeof(app::espnow::state_binary::MasterNetState))) {
          const auto* state = reinterpret_cast<const app::espnow::state_binary::MasterNetState*>(payload);
          ESP_LOGI("MASTER", "Internet=%s channel=%u", state->online == 1 ? "UP" : "DOWN", state->channel);
        }
      }
    default:
      break;
  }
}

}  // namespace app::espnow
