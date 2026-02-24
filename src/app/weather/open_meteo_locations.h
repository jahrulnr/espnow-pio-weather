#pragma once

#include <Arduino.h>

namespace app::weather {

enum class Area : uint8_t {
  JakartaPusat = 0,
  JakartaBarat,
  JakartaTimur,
  JakartaUtara,
  JakartaSelatan,
  Bogor,
  Depok,
  Tangerang,
  Bekasi,
  Pekanbaru,
  BaganJaya,
};

struct Coordinates {
  double latitude;
  double longitude;
};

bool getCoordinates(Area area, Coordinates& out);
const char* toString(Area area);
String buildCurrentWeatherUrl(Area area);
bool saveLastReport(const String& report);
bool loadLastReport(String& report);

}  // namespace app::weather
