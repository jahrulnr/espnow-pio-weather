#include "networkTask.h"

#include "app/espnow/slave.h"
#include "app/espnow/state_binary.h"
#include "app/weather/open_meteo_locations.h"
#include "app/espnow/payload_codec.h"

#include <app_config.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <esp_log.h>

namespace app::tasks {

namespace {

static constexpr uint16_t NETWORK_TASK_STACK = 8192;
static constexpr UBaseType_t NETWORK_TASK_PRIORITY = 2;
static constexpr uint32_t kWeatherRefreshIntervalMs = 60UL * 60UL * 1000UL; // 1h
// use macro WEATHER_PROXY_REQUEST_INTERVAL_MS from app_config.h for proxy interval
static constexpr size_t OUTGOING_QUEUE_DEPTH = 10;

struct OutgoingJob {
  uint8_t payload[app::espnow::MAX_PAYLOAD_SIZE];
  uint16_t payloadSize;
  bool isText;
};

TaskHandle_t networkTaskHandle = nullptr;
QueueHandle_t outgoingQueue = nullptr;

void sendIdentityStateNow() {
  app::espnow::state_binary::IdentityState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Identity);
  strncpy(state.id, DEVICE_NAME, sizeof(state.id) - 1);
  app::espnow::espnowSlave.sendStateBinary(&state, sizeof(state));
}

void sendFeaturesStateNow() {
  app::espnow::state_binary::FeaturesState state = {};
  app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Features);
  state.contractVersion = 1;
  state.featureBits = static_cast<uint32_t>(app::espnow::state_binary::FeatureIdentity)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureSensor)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureWeather)
                   | static_cast<uint32_t>(app::espnow::state_binary::FeatureProxyClient);
  app::espnow::espnowSlave.sendStateBinary(&state, sizeof(state));
}

void publishProxyRequestNow(const String& url) {
  app::espnow::state_binary::ProxyReqState request = {};
  app::espnow::state_binary::initHeader(request.header, app::espnow::state_binary::Type::ProxyReq);
  request.method = static_cast<uint8_t>(app::espnow::state_binary::HttpMethod::Get);
  strncpy(request.url, url.c_str(), sizeof(request.url) - 1);
  app::espnow::espnowSlave.sendStateBinary(&request, sizeof(request));
}

void networkTaskRunner(void*) {
  // start espnow radio
  app::espnow::espnowSlave.begin(app::espnow::DEFAULT_CHANNEL);

  // prepare outgoing queue
  outgoingQueue = xQueueCreate(OUTGOING_QUEUE_DEPTH, sizeof(OutgoingJob));
  if (outgoingQueue == nullptr) {
    ESP_LOGE("NET_TASK", "Failed creating outgoing queue");
  }

  // weather cache/load
  String cachedProxyRequest;
  String cachedWeatherUrl;
  if (app::weather::loadLastReport(cachedProxyRequest)) {
    if (!cachedProxyRequest.startsWith("state=proxy_req") ||
        !app::espnow::codec::getField(cachedProxyRequest, "url", cachedWeatherUrl) || cachedWeatherUrl.isEmpty()) {
      cachedProxyRequest = "";
      cachedWeatherUrl = "";
    }
  }

  uint32_t lastWeatherRefreshMs = 0;
  uint32_t lastWeatherRequestMs = 0;
  bool wasMasterLinked = false;

  if (cachedWeatherUrl.isEmpty()) {
    const auto area = static_cast<app::weather::Area>(WEATHER_AREA_INDEX);
    cachedWeatherUrl = app::weather::buildCurrentWeatherUrl(area);
    cachedProxyRequest = app::espnow::codec::buildPayload({
        {"state", "proxy_req"},
        {"method", "GET"},
        {"url", cachedWeatherUrl},
        {"payload", "{}"},
    });
    app::weather::saveLastReport(cachedProxyRequest);
  }

  // send initial proxy request (bootstrap)
  publishProxyRequestNow(cachedWeatherUrl);
  lastWeatherRefreshMs = millis();
  lastWeatherRequestMs = millis();

  while (true) {
    app::espnow::espnowSlave.loop();

    // handle outgoing queue
    if (outgoingQueue != nullptr) {
      OutgoingJob job;
      if (xQueueReceive(outgoingQueue, &job, 0) == pdTRUE) {
        if (job.payloadSize > 0) {
          app::espnow::espnowSlave.sendStateBinary(job.payload, job.payloadSize);
        }
      }
    }

    // handle master link events and periodic proxy requests
    const uint32_t now = millis();
    const bool isMasterLinked = app::espnow::espnowSlave.isMasterLinked();
    if (isMasterLinked && !wasMasterLinked) {
      sendIdentityStateNow();
      sendFeaturesStateNow();
      if (cachedWeatherUrl.isEmpty()) {
        const auto area = static_cast<app::weather::Area>(WEATHER_AREA_INDEX);
        cachedWeatherUrl = app::weather::buildCurrentWeatherUrl(area);
      }
      publishProxyRequestNow(cachedWeatherUrl);
      lastWeatherRequestMs = now;
    }
    wasMasterLinked = isMasterLinked;

    if (now - lastWeatherRefreshMs >= kWeatherRefreshIntervalMs) {
      const auto area = static_cast<app::weather::Area>(WEATHER_AREA_INDEX);
      cachedWeatherUrl = app::weather::buildCurrentWeatherUrl(area);
      cachedProxyRequest = app::espnow::codec::buildPayload({
          {"state", "proxy_req"},
          {"method", "GET"},
          {"url", cachedWeatherUrl},
          {"payload", "{}"},
      });
      app::weather::saveLastReport(cachedProxyRequest);
      lastWeatherRefreshMs = now;
    }

    if (!cachedProxyRequest.isEmpty() && (now - lastWeatherRequestMs >= static_cast<uint32_t>(WEATHER_PROXY_REQUEST_INTERVAL_MS))) {
      publishProxyRequestNow(cachedWeatherUrl);
      lastWeatherRequestMs = now;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

bool startNetworkTask() {
  if (networkTaskHandle != nullptr) {
    return true;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
      networkTaskRunner,
      "network_task",
      NETWORK_TASK_STACK,
      nullptr,
      NETWORK_TASK_PRIORITY,
      &networkTaskHandle,
      tskNO_AFFINITY);

  if (created != pdPASS) {
    ESP_LOGE("NET_TASK", "Failed to start network task");
    networkTaskHandle = nullptr;
    return false;
  }

  ESP_LOGI("NET_TASK", "Network task started");
  return true;
}

bool publishOutgoingBinary(const void* payload, size_t payloadSize) {
  if (payload == nullptr || payloadSize == 0 || payloadSize > app::espnow::MAX_PAYLOAD_SIZE || outgoingQueue == nullptr) {
    return false;
  }

  OutgoingJob job;
  memset(&job, 0, sizeof(job));
  memcpy(job.payload, payload, payloadSize);
  job.payloadSize = static_cast<uint16_t>(payloadSize);
  job.isText = false;

  if (xQueueSend(outgoingQueue, &job, 0) != pdTRUE) {
    return false;
  }
  return true;
}

bool publishOutgoingText(const String& text) {
  if (text.isEmpty()) {
    return false;
  }
  return publishOutgoingBinary(text.c_str(), text.length());
}

}  // namespace app::tasks
