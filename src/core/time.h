#pragma once

#include <Arduino.h>

class TimeManager {
public:
  TimeManager(){};

  inline static TimeManager& getInstance() {
    static TimeManager instance;
    return instance;
  }

  inline bool init() {
    if (initialized) {
      return true;
    }

    // Set timezone to Asia/Jakarta (UTC+7, no DST)
    configTime(25200, 0, "pool.ntp.org", "time.google.com");

    initialized = true;
    return true;
  }

  inline bool syncTime() {
    if (!initialized) {
      return false;
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10000)) { // 10 second timeout
      ESP_LOGE("TIME", "Failed to sync time with NTP");
      return false;
    }

    ESP_LOGI("TIME", "Time synced successfully");
    return true;
  }

  inline const char* getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return "Time not synced";
    }

    memset(buffer, 0, sizeof(buffer));
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S WIB", &timeinfo);
    return buffer;
  }

  inline int getHour() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return -1;
    }
    return timeinfo.tm_hour;
  }

  inline int getMinutes() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return -1;
    }
    return timeinfo.tm_min;
  }

  inline int getSeconds() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      return -1;
    }
    return timeinfo.tm_sec;
  }


private:
  bool initialized;
  char buffer[64];
};


#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_LITTLEFS)
extern TimeManager timeManager;
#endif