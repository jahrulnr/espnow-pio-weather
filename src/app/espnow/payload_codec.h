#pragma once

#include <Arduino.h>
#include <initializer_list>

namespace app::espnow::codec {

static constexpr const char* kSeparator = "|---|";

struct Field {
  const char* key;
  String value;
};

inline String buildPayload(std::initializer_list<Field> fields) {
  String payload;
  bool first = true;

  for (const auto& field : fields) {
    if (field.key == nullptr || field.value.isEmpty()) {
      continue;
    }

    if (!first) {
      payload += kSeparator;
    }

    payload += field.key;
    payload += "=";
    payload += field.value;
    first = false;
  }

  return payload;
}

inline bool getField(const String& source, const char* key, String& out) {
  out = "";
  if (key == nullptr) {
    return false;
  }

  const String marker = String(key) + "=";
  const int begin = source.indexOf(marker);
  if (begin < 0) {
    return false;
  }

  const int valueStart = begin + marker.length();
  int valueEnd = source.indexOf(kSeparator, valueStart);
  if (valueEnd < 0) {
    valueEnd = source.length();
  }

  out = source.substring(valueStart, valueEnd);
  out.trim();
  return out.length() > 0;
}

}  // namespace app::espnow::codec
