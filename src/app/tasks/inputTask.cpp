#include "inputTask.h"

#include "app/espnow/payload_codec.h"
#include "app/input/battery/battery_manager.h"
#include "app/sensor/dht_sensor.h"
#include "app/tasks/networkTask.h"
#include "app/espnow/state_binary.h"

#include <app_config.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace app::tasks {

namespace {

static constexpr const char* TAG = "INPUT_TASK";
static constexpr uint16_t INPUT_TASK_STACK = 4096;
static constexpr UBaseType_t INPUT_TASK_PRIORITY = 1;
static constexpr uint32_t INPUT_POLL_INTERVAL_MS = 20;
static constexpr uint32_t BATTERY_PUBLISH_INTERVAL_MS = 1000;

TaskHandle_t inputTaskHandle = nullptr;
BatteryManager batteryManager;

static uint32_t lastDhtReadMs = 0;

uint32_t lastBatteryPublishMs = 0;
int lastPublishedBatteryLevel = -1;

void publishBatterySnapshotToDisplay() {
  batteryManager.update();

  const uint32_t now = millis();
  if (lastBatteryPublishMs != 0 && (now - lastBatteryPublishMs) < BATTERY_PUBLISH_INTERVAL_MS) {
    return;
  }

  const int batteryLevel = batteryManager.getLevel();
  if (batteryLevel < 0 || batteryLevel > 100) {
    return;
  }

  if (batteryLevel == lastPublishedBatteryLevel && lastBatteryPublishMs != 0) {
    return;
  }

  const String payload = app::espnow::codec::buildPayload({
      {"batt", String(batteryLevel)},
  });

  lastPublishedBatteryLevel = batteryLevel;
  lastBatteryPublishMs = now;
}

void inputTaskRunner(void*) {
  batteryManager.init(INPUT_BATTERY_ADC_PIN);
  batteryManager.setVoltage(3.3f, 4.2f, 2.0f);
  batteryManager.setUpdateInterval(5000);
  #if DHT_SENSOR_ENABLED
  app::sensor::dhtSensor.begin(DHT_SENSOR_PIN, DHT_SENSOR_IS_DHT22 == 1);
  lastDhtReadMs = millis();
  #endif
  
  publishBatterySnapshotToDisplay();

  while (true) {
    publishBatterySnapshotToDisplay();

    #if DHT_SENSOR_ENABLED
    const uint32_t now = millis();
    if (now - lastDhtReadMs >= DHT_READ_INTERVAL_MS) {
      app::sensor::DhtReading reading;
      if (app::sensor::dhtSensor.read(reading) && reading.valid) {
        app::espnow::state_binary::SensorState state = {};
        app::espnow::state_binary::initHeader(state.header, app::espnow::state_binary::Type::Sensor);
        state.temperature10 = static_cast<int16_t>(reading.temperatureC * 10.0f);
        state.humidity10 = static_cast<uint16_t>(reading.humidityPercent * 10.0f);
			  ESP_LOGI("DHT", "sensor temp=%.1fC hum=%.1f%%", reading.temperatureC, reading.humidityPercent);
        // enqueue to network task for sending via ESP-NOW
        app::tasks::publishOutgoingBinary(&state, sizeof(state));
      }
      lastDhtReadMs = now;
    }
    #endif

    vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_INTERVAL_MS));
  }
}

}  // namespace

bool startInputTask() {
  if (inputTaskHandle != nullptr) {
    return true;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
      inputTaskRunner,
      "input_task",
      INPUT_TASK_STACK,
      nullptr,
      INPUT_TASK_PRIORITY,
      &inputTaskHandle,
      tskNO_AFFINITY);

  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed to start input task");
    inputTaskHandle = nullptr;
    return false;
  }

  ESP_LOGI(TAG, "Input task started");
  return true;
}

}  // namespace app::tasks
