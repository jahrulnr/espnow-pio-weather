#include "open_meteo_locations.h"

#include <LittleFS.h>
#include <esp_log.h>

namespace app::weather {

static constexpr const char* TAG = "weather_area";
static constexpr const char* OPEN_METEO_BASE = "https://api.open-meteo.com/v1/forecast";
static constexpr const char* WEATHER_REPORT_PATH = "/data/weather_last_report.txt";

bool getCoordinates(Area area, Coordinates& out) {
  switch (area) {
    case Area::JakartaPusat:
      out = {.latitude = -6.1805000, .longitude = 106.8283000};
      return true;
    case Area::JakartaBarat:
      out = {.latitude = -6.1905425, .longitude = 106.7602213};
      return true;
    case Area::JakartaTimur:
      out = {.latitude = -6.2250000, .longitude = 106.9004000};
      return true;
    case Area::JakartaUtara:
      out = {.latitude = -6.1380000, .longitude = 106.8636000};
      return true;
    case Area::JakartaSelatan:
      out = {.latitude = -6.2615000, .longitude = 106.8106000};
      return true;
    case Area::Bogor:
      out = {.latitude = -6.595038, .longitude = 106.816635};
      return true;
    case Area::Depok:
      out = {.latitude = -6.402484, .longitude = 106.794241};
      return true;
    case Area::Tangerang:
      out = {.latitude = -6.178306, .longitude = 106.631889};
      return true;
    case Area::Bekasi:
      out = {.latitude = -6.241586, .longitude = 106.992416};
      return true;
    case Area::Pekanbaru:
      out = {.latitude = 0.507068, .longitude = 101.447777};
      return true;
    case Area::BaganJaya:
      out = {.latitude = 2.161847, .longitude = 100.811737};
      return true;
    default:
      return false;
  }
}

const char* toString(Area area) {
  switch (area) {
    case Area::JakartaPusat:
      return "jakarta_pusat";
    case Area::JakartaBarat:
      return "jakarta_barat";
    case Area::JakartaTimur:
      return "jakarta_timur";
    case Area::JakartaUtara:
      return "jakarta_utara";
    case Area::JakartaSelatan:
      return "jakarta_selatan";
    case Area::Bogor:
      return "bogor";
    case Area::Depok:
      return "depok";
    case Area::Tangerang:
      return "tangerang";
    case Area::Bekasi:
      return "bekasi";
    case Area::Pekanbaru:
      return "pekanbaru";
    case Area::BaganJaya:
      return "bagan_jaya";
    default:
      return "unknown";
  }
}

String buildCurrentWeatherUrl(Area area) {
  Coordinates coordinates = {};
  if (!getCoordinates(area, coordinates)) {
    ESP_LOGW(TAG, "Unknown area enum (%u), fallback to Jakarta Pusat", static_cast<unsigned>(area));
    coordinates = {.latitude = -6.1805000, .longitude = 106.8283000};
  }

  String url = OPEN_METEO_BASE;
  url += "?latitude=";
  url += String(coordinates.latitude, 7);
  url += "&longitude=";
  url += String(coordinates.longitude, 7);
  url += "&current_weather=true";
  return url;
}

bool saveLastReport(const String& report) {
  if (!LittleFS.exists("/data")) {
    LittleFS.mkdir("/data");
  }

  File file = LittleFS.open(WEATHER_REPORT_PATH, "w");
  if (!file) {
    ESP_LOGW(TAG, "Failed to open weather report file for write");
    return false;
  }

  file.print(report);
  file.close();
  return true;
}

bool loadLastReport(String& report) {
  report = "";

  if (!LittleFS.exists(WEATHER_REPORT_PATH)) {
    return false;
  }

  File file = LittleFS.open(WEATHER_REPORT_PATH, "r");
  if (!file) {
    ESP_LOGW(TAG, "Failed to open weather report file for read");
    return false;
  }

  report = file.readString();
  file.close();
  return report.length() > 0;
}

}  // namespace app::weather
