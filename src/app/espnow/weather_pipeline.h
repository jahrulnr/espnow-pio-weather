#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "protocol.h"

namespace app::espnow {

class IStateSink {
 public:
  virtual ~IStateSink() = default;
  virtual bool publishState(const String& payload) = 0;
};

class ICommandTaskSink {
 public:
  virtual ~ICommandTaskSink() = default;
  virtual bool submitCommand(const uint8_t* payload, size_t payloadSize) = 0;
};

class WeatherCommandPipeline : public ICommandTaskSink {
 public:
  WeatherCommandPipeline() = default;

  void injectStateSink(IStateSink* sink);
  bool begin();
  bool submitCommand(const uint8_t* payload, size_t payloadSize) override;

 private:
  static constexpr uint8_t kQueueDepth = 10;
  static constexpr size_t kMaxAssembledBytes = 1024;
  static constexpr uint16_t kTaskStackWords = 6144;
  static constexpr UBaseType_t kTaskPriority = 2;

  struct CommandJob {
    uint8_t payload[MAX_PAYLOAD_SIZE] = {0};
    uint8_t payloadSize = 0;
  };

  struct ChunkState {
    String id;
    uint16_t total = 0;
    uint16_t nextIndex = 1;
    String buffer;
  };

  static void taskEntry(void* context);
  void taskLoop();
  void handleCommand(const uint8_t* payload, uint8_t payloadSize);
  void handleProxyPayload(uint8_t ok, int16_t code, const String& responseBody);
  bool parseWeatherFields(const String& proxyData,
                          String& weatherCode,
                          String& time,
                          String& temperature,
                          String& windspeed,
                          String& winddirection);
  bool extractJsonObject(const String& json, const char* key, String& objectOut);
  bool extractJsonField(const String& objectText, const char* field, String& valueOut);

  IStateSink* stateSink = nullptr;
  QueueHandle_t queue = nullptr;
  TaskHandle_t task = nullptr;
  ChunkState chunk;
};

}  // namespace app::espnow
