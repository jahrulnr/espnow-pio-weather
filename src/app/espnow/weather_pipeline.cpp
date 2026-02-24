#include "weather_pipeline.h"

#include "payload_codec.h"
#include "state_binary.h"
#include <app_config.h>

#include <esp_log.h>

namespace app::espnow {

namespace {

static constexpr const char* TAG = "weather_pipe";

}  // namespace

void WeatherCommandPipeline::injectStateSink(IStateSink* sink) {
  stateSink = sink;
}

bool WeatherCommandPipeline::begin() {
  if (queue != nullptr && task != nullptr) {
    return true;
  }

  queue = xQueueCreate(kQueueDepth, sizeof(CommandJob));
  if (queue == nullptr) {
    ESP_LOGE(TAG, "Failed creating command queue");
    return false;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry,
      "weather_pipe",
      kTaskStackWords,
      this,
      kTaskPriority,
      &task,
      tskNO_AFFINITY);

  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed creating weather pipeline task");
    return false;
  }

  ESP_LOGI(TAG, "Weather pipeline ready");
  return true;
}

bool WeatherCommandPipeline::submitCommand(const uint8_t* payload, size_t payloadSize) {
  if (queue == nullptr || payload == nullptr || payloadSize == 0 || payloadSize > MAX_PAYLOAD_SIZE) {
    return false;
  }

  CommandJob job;
  memcpy(job.payload, payload, payloadSize);
  job.payloadSize = static_cast<uint8_t>(payloadSize);

  if (xQueueSend(queue, &job, 0) != pdTRUE) {
    ESP_LOGW(TAG, "Command queue full, dropping payload");
    return false;
  }

  return true;
}

void WeatherCommandPipeline::taskEntry(void* context) {
  auto* self = static_cast<WeatherCommandPipeline*>(context);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }

  self->taskLoop();
}

void WeatherCommandPipeline::taskLoop() {
  CommandJob job;
  while (true) {
    if (xQueueReceive(queue, &job, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    handleCommand(job.payload, job.payloadSize);
  }
}

void WeatherCommandPipeline::handleCommand(const uint8_t* payload, uint8_t payloadSize) {
  if (!app::espnow::state_binary::hasTypeAndSize(payload,
                                                  payloadSize,
                                                  app::espnow::state_binary::Type::ProxyRespChunk,
                                                  sizeof(app::espnow::state_binary::ProxyRespChunkCommand))) {
    ESP_LOGW(TAG, "Invalid command struct, ignored");
    return;
  }

  const auto* command = reinterpret_cast<const app::espnow::state_binary::ProxyRespChunkCommand*>(payload);
  if (command->idx == 0 || command->total == 0 || command->idx > command->total ||
      command->dataLen > app::espnow::state_binary::kProxyChunkDataBytes) {
    ESP_LOGW(TAG, "Invalid proxy chunk fields");
    return;
  }

  const String dataChunk(reinterpret_cast<const char*>(command->data), command->dataLen);

  if (command->idx == 1 || String(command->requestId) != chunk.id) {
    chunk.id = String(command->requestId);
    chunk.total = command->total;
    chunk.nextIndex = 1;
    chunk.buffer = "";
  }

  if (String(command->requestId) != chunk.id || command->total != chunk.total || command->idx != chunk.nextIndex) {
    ESP_LOGW(TAG,
             "Out-of-order chunk id=%u idx=%u expected=%u",
             command->requestId,
             command->idx,
             chunk.nextIndex);
    return;
  }

  if (chunk.buffer.length() + dataChunk.length() > kMaxAssembledBytes) {
    ESP_LOGW(TAG, "Chunk assembly overflow, reset");
    chunk = ChunkState{};
    return;
  }

  chunk.buffer += dataChunk;
  chunk.nextIndex++;

  if (command->idx == command->total) {
    ESP_LOGI(TAG, "Chunk assemble complete (%u chunks)", command->total);
    handleProxyPayload(command->ok, command->code, chunk.buffer);
    chunk = ChunkState{};
  }
}

void WeatherCommandPipeline::handleProxyPayload(uint8_t ok, int16_t code, const String& responseBody) {
  ESP_LOGI("WEATHER", "Proxy result ok=%u code=%d", ok, code);

  String weatherCode;
  String time;
  String temperature;
  String windspeed;
  String winddirection;
  const bool hasFields = parseWeatherFields(responseBody, weatherCode, time, temperature, windspeed, winddirection);
  if (!hasFields) {
    ESP_LOGW("WEATHER", "No current_weather fields parsed");
    return;
  }

  const String weatherState = app::espnow::codec::buildPayload({
      {"state", "weather"},
      {"ok", String(ok)},
			{"code", weatherCode},
      {"time", time},
      {"temperature", temperature},
      {"windspeed", windspeed},
      {"winddirection", winddirection},
  });

  if (stateSink == nullptr) {
    ESP_LOGW(TAG, "State sink not injected");
    return;
  }

  if (stateSink->publishState(weatherState)) {
    ESP_LOGI("WEATHER", "Forwarded weather state to master");
  } else {
    ESP_LOGW("WEATHER", "Failed forwarding weather state to master");
  }
}

bool WeatherCommandPipeline::parseWeatherFields(const String& proxyData,
                                                String& weatherCode,
                                                String& time,
                                                String& temperature,
                                                String& windspeed,
                                                String& winddirection) {
  weatherCode = "";
  time = "";
  temperature = "";
  windspeed = "";
  winddirection = "";

  String currentWeatherObject;
  if (!extractJsonObject(proxyData, "current_weather", currentWeatherObject)) {
    return false;
  }

  const bool hasWeatherCode = extractJsonField(currentWeatherObject, "weathercode", weatherCode);
  const bool hasTime = extractJsonField(currentWeatherObject, "time", time);
  const bool hasTemperature = extractJsonField(currentWeatherObject, "temperature", temperature);
  const bool hasWindspeed = extractJsonField(currentWeatherObject, "windspeed", windspeed);
  const bool hasWinddirection = extractJsonField(currentWeatherObject, "winddirection", winddirection);
  return hasWeatherCode || hasTime || hasTemperature || hasWindspeed || hasWinddirection;
}

bool WeatherCommandPipeline::extractJsonObject(const String& json, const char* key, String& objectOut) {
  objectOut = "";
  if (key == nullptr) {
    return false;
  }

  const String keyToken = String("\"") + key + "\"";
  const int keyPos = json.indexOf(keyToken);
  if (keyPos < 0) {
    return false;
  }

  const int braceStart = json.indexOf('{', keyPos);
  if (braceStart < 0) {
    return false;
  }

  int depth = 0;
  for (int index = braceStart; index < static_cast<int>(json.length()); ++index) {
    const char ch = json[index];
    if (ch == '{') {
      depth++;
    } else if (ch == '}') {
      depth--;
      if (depth == 0) {
        objectOut = json.substring(braceStart, index + 1);
        return true;
      }
    }
  }

  return false;
}

bool WeatherCommandPipeline::extractJsonField(const String& objectText, const char* field, String& valueOut) {
  valueOut = "";
  if (field == nullptr) {
    return false;
  }

  const String marker = String("\"") + field + "\"";
  const int fieldPos = objectText.indexOf(marker);
  if (fieldPos < 0) {
    return false;
  }

  const int colonPos = objectText.indexOf(':', fieldPos + marker.length());
  if (colonPos < 0) {
    return false;
  }

  int start = colonPos + 1;
  while (start < static_cast<int>(objectText.length()) &&
         (objectText[start] == ' ' || objectText[start] == '\t')) {
    start++;
  }

  if (start >= static_cast<int>(objectText.length())) {
    return false;
  }

  if (objectText[start] == '"') {
    const int endQuote = objectText.indexOf('"', start + 1);
    if (endQuote <= start) {
      return false;
    }
    valueOut = objectText.substring(start + 1, endQuote);
    return !valueOut.isEmpty();
  }

  int end = start;
  while (end < static_cast<int>(objectText.length()) && objectText[end] != ',' && objectText[end] != '}') {
    end++;
  }

  valueOut = objectText.substring(start, end);
  valueOut.trim();
  return !valueOut.isEmpty();
}

}  // namespace app::espnow
