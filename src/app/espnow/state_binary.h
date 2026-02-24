#pragma once

#include <Arduino.h>

namespace app::espnow::state_binary {

static constexpr uint8_t kMagic = 0xB1;
static constexpr uint8_t kVersion = 1;

enum class Type : uint8_t {
  Identity = 1,
  Sensor = 2,
  ProxyReq = 3,
  Weather = 4,
  MasterNet = 5,
  SlaveAlive = 6,
  ProxyRespChunk = 7,
  WeatherSyncReq = 8,
  Features = 9,
  IdentityReq = 10,
};

enum Feature : uint32_t {
  FeatureIdentity = 1UL << 0,
  FeatureSensor = 1UL << 1,
  FeatureWeather = 1UL << 2,
  FeatureProxyClient = 1UL << 3,
  FeatureCameraJpeg = 1UL << 4,
  FeatureCameraStream = 1UL << 5,
  FeatureControlBasic = 1UL << 6,
};

enum class HttpMethod : uint8_t {
  Get = 1,
  Post = 2,
  Patch = 3,
};

struct __attribute__((packed)) Header {
  uint8_t magic;
  uint8_t version;
  uint8_t type;
  uint8_t reserved;
};

struct __attribute__((packed)) IdentityState {
  Header header;
  char id[24];
};

struct __attribute__((packed)) SensorState {
  Header header;
  int16_t temperature10;
  uint16_t humidity10;
};

struct __attribute__((packed)) ProxyReqState {
  Header header;
  uint8_t method;
  char url[140];
};

struct __attribute__((packed)) WeatherState {
  Header header;
  uint8_t ok;
  int16_t code;
  char time[20];
  int16_t temperature10;
  int16_t windspeed10;
  uint16_t winddirection;
};

struct __attribute__((packed)) MasterNetState {
  Header header;
  uint8_t online;
  uint8_t channel;
};

struct __attribute__((packed)) SlaveAliveState {
  Header header;
};

struct __attribute__((packed)) FeaturesState {
  Header header;
  uint32_t featureBits;
  uint16_t contractVersion;
  uint16_t reserved;
};

static constexpr size_t kProxyChunkDataBytes = 160;

struct __attribute__((packed)) ProxyRespChunkCommand {
  Header header;
  uint16_t requestId;
  uint16_t idx;
  uint16_t total;
  uint8_t ok;
  int16_t code;
  uint8_t dataLen;
  uint8_t data[kProxyChunkDataBytes];
};

struct __attribute__((packed)) WeatherSyncReqCommand {
  Header header;
  uint8_t force;
};

struct __attribute__((packed)) IdentityReqCommand {
  Header header;
};

inline void initHeader(Header& header, Type type) {
  header.magic = kMagic;
  header.version = kVersion;
  header.type = static_cast<uint8_t>(type);
  header.reserved = 0;
}

inline bool hasValidHeader(const uint8_t* payload, size_t payloadSize) {
  if (payload == nullptr || payloadSize < sizeof(Header)) {
    return false;
  }

  const auto* header = reinterpret_cast<const Header*>(payload);
  return header->magic == kMagic && header->version == kVersion;
}

inline bool hasTypeAndSize(const uint8_t* payload, size_t payloadSize, Type expectedType, size_t expectedSize) {
  if (!hasValidHeader(payload, payloadSize) || payloadSize != expectedSize) {
    return false;
  }

  const auto* header = reinterpret_cast<const Header*>(payload);
  return header->type == static_cast<uint8_t>(expectedType);
}

}  // namespace app::espnow::state_binary
