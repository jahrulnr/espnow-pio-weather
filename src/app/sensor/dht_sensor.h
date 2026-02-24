#pragma once

#include <Arduino.h>

namespace app::sensor {

struct DhtReading {
  float temperatureC = 0.0f;
  float humidityPercent = 0.0f;
  bool valid = false;
};

class DhtSensor {
 public:
  DhtSensor() = default;

  bool begin(uint8_t pin, bool isDht22 = true);
  bool read(DhtReading& out);

 private:
  bool waitForLevel(uint8_t expectedLevel, uint32_t timeoutUs);

  uint8_t dataPin = 255;
  bool dht22 = true;
  bool started = false;
};

extern DhtSensor dhtSensor;

}  // namespace app::sensor
