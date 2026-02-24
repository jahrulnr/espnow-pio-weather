#pragma once

#include <stdint.h>

namespace app::weather {

enum class OpenMeteoWeatherCode : int16_t {
  ClearSky = 0,
  MainlyClear = 1,
  PartlyCloudy = 2,
  Overcast = 3,
  Fog = 45,
  DepositingRimeFog = 48,
  DrizzleLight = 51,
  DrizzleModerate = 53,
  DrizzleDense = 55,
  FreezingDrizzleLight = 56,
  FreezingDrizzleDense = 57,
  RainSlight = 61,
  RainModerate = 63,
  RainHeavy = 65,
  FreezingRainLight = 66,
  FreezingRainHeavy = 67,
  SnowFallSlight = 71,
  SnowFallModerate = 73,
  SnowFallHeavy = 75,
  SnowGrains = 77,
  RainShowersSlight = 80,
  RainShowersModerate = 81,
  RainShowersViolent = 82,
  SnowShowersSlight = 85,
  SnowShowersHeavy = 86,
  Thunderstorm = 95,
  ThunderstormWithSlightHail = 96,
  ThunderstormWithHeavyHail = 99,
};

inline const char* toGoogleWeatherV2Icon(OpenMeteoWeatherCode code) {
  switch (code) {
    case OpenMeteoWeatherCode::ClearSky:
      return "sunny.png";
    case OpenMeteoWeatherCode::MainlyClear:
      return "mostly_sunny.png";
    case OpenMeteoWeatherCode::PartlyCloudy:
      return "partly_cloudy.png";
    case OpenMeteoWeatherCode::Overcast:
      return "cloudy.png";
    case OpenMeteoWeatherCode::Fog:
    case OpenMeteoWeatherCode::DepositingRimeFog:
      return "haze_fog_dust_smoke.png";
    case OpenMeteoWeatherCode::DrizzleLight:
    case OpenMeteoWeatherCode::DrizzleModerate:
      return "drizzle.png";
    case OpenMeteoWeatherCode::DrizzleDense:
      return "showers_rain.png";
    case OpenMeteoWeatherCode::FreezingDrizzleLight:
    case OpenMeteoWeatherCode::FreezingDrizzleDense:
    case OpenMeteoWeatherCode::FreezingRainLight:
    case OpenMeteoWeatherCode::FreezingRainHeavy:
      return "wintry_mix_rain_snow.png";
    case OpenMeteoWeatherCode::RainSlight:
    case OpenMeteoWeatherCode::RainModerate:
    case OpenMeteoWeatherCode::RainShowersSlight:
    case OpenMeteoWeatherCode::RainShowersModerate:
      return "showers_rain.png";
    case OpenMeteoWeatherCode::RainHeavy:
    case OpenMeteoWeatherCode::RainShowersViolent:
      return "heavy_rain.png";
    case OpenMeteoWeatherCode::SnowFallSlight:
      return "flurries.png";
    case OpenMeteoWeatherCode::SnowFallModerate:
    case OpenMeteoWeatherCode::SnowGrains:
    case OpenMeteoWeatherCode::SnowShowersSlight:
      return "snow_showers_snow.png";
    case OpenMeteoWeatherCode::SnowFallHeavy:
    case OpenMeteoWeatherCode::SnowShowersHeavy:
      return "heavy_snow.png";
    case OpenMeteoWeatherCode::Thunderstorm:
      return "strong_tstorms.png";
    case OpenMeteoWeatherCode::ThunderstormWithSlightHail:
    case OpenMeteoWeatherCode::ThunderstormWithHeavyHail:
      return "sleet_hail.png";
  }

  return "cloudy.png";
}

inline const char* toGoogleWeatherV2Icon(int code) {
  return toGoogleWeatherV2Icon(static_cast<OpenMeteoWeatherCode>(code));
}

}  // namespace app::weather
