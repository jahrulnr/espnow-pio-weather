#include "dht_sensor.h"

#include <esp_log.h>

namespace app::sensor {

static const char* TAG = "dht_sensor";

DhtSensor dhtSensor;

bool DhtSensor::begin(uint8_t pin, bool isDht22) {
  if (pin == 255) {
    ESP_LOGE(TAG, "Invalid DHT GPIO pin");
    return false;
  }

  dataPin = pin;
  dht22 = isDht22;
  started = true;

  pinMode(dataPin, INPUT_PULLUP);
  ESP_LOGI(TAG, "DHT sensor initialized at GPIO %u (%s)", dataPin, dht22 ? "DHT22" : "DHT11");
  return true;
}

bool DhtSensor::waitForLevel(uint8_t expectedLevel, uint32_t timeoutUs) {
  const uint32_t start = micros();
  while (digitalRead(dataPin) != expectedLevel) {
    if ((micros() - start) > timeoutUs) {
      return false;
    }
  }
  return true;
}

bool DhtSensor::read(DhtReading& out) {
  out.valid = false;
  if (!started) {
    return false;
  }

  uint8_t raw[5] = {0, 0, 0, 0, 0};

  pinMode(dataPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  delay(dht22 ? 2 : 20);
  noInterrupts();
  digitalWrite(dataPin, HIGH);
  delayMicroseconds(30);
  pinMode(dataPin, INPUT_PULLUP);

  if (!waitForLevel(LOW, 100)) {
    interrupts();
    ESP_LOGW(TAG, "Timeout waiting DHT response (LOW)");
    return false;
  }

  if (!waitForLevel(HIGH, 100)) {
    interrupts();
    ESP_LOGW(TAG, "Timeout waiting DHT response (HIGH)");
    return false;
  }

  if (!waitForLevel(LOW, 100)) {
    interrupts();
    ESP_LOGW(TAG, "Timeout waiting DHT data start");
    return false;
  }

  for (int bitIndex = 0; bitIndex < 40; ++bitIndex) {
    if (!waitForLevel(HIGH, 70)) {
      interrupts();
      ESP_LOGW(TAG, "Timeout waiting DHT bit high");
      return false;
    }

    const uint32_t pulseStart = micros();
    if (!waitForLevel(LOW, 120)) {
      interrupts();
      ESP_LOGW(TAG, "Timeout waiting DHT bit low");
      return false;
    }

    const uint32_t pulseWidth = micros() - pulseStart;
    const uint8_t byteIndex = static_cast<uint8_t>(bitIndex / 8);
    raw[byteIndex] <<= 1;
    if (pulseWidth > 45) {
      raw[byteIndex] |= 1;
    }
  }

  interrupts();

  const uint8_t checksum = static_cast<uint8_t>(raw[0] + raw[1] + raw[2] + raw[3]);
  if (checksum != raw[4]) {
    ESP_LOGW(TAG, "Checksum mismatch: got=%u expected=%u", raw[4], checksum);
    return false;
  }

  if (dht22) {
    uint16_t rawHum = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
    uint16_t rawTemp = (static_cast<uint16_t>(raw[2]) << 8) | raw[3];

    out.humidityPercent = static_cast<float>(rawHum) * 0.1f;
    out.temperatureC = static_cast<float>(rawTemp & 0x7FFF) * 0.1f;
    if (rawTemp & 0x8000) {
      out.temperatureC = -out.temperatureC;
    }
  } else {
    out.humidityPercent = static_cast<float>(raw[0]);
    out.temperatureC = static_cast<float>(raw[2]);
  }

  out.valid = true;
  return true;
}

}  // namespace app::sensor
