#pragma once

#include <Arduino.h>

namespace app::espnow {

enum class PacketType : uint8_t {
  HELLO = 1,
  HEARTBEAT = 2,
  COMMAND = 3,
  STATE = 4,
};

static constexpr uint8_t PROTOCOL_VERSION = 1;
static constexpr uint8_t DEFAULT_CHANNEL = 1;
static constexpr size_t MAX_PAYLOAD_SIZE = 200;
static constexpr char MASTER_BEACON_ID[] = "PIO_MASTER_V1";
static constexpr size_t MASTER_BEACON_ID_LEN = sizeof(MASTER_BEACON_ID) - 1;

struct __attribute__((packed)) PacketHeader {
  uint8_t version;
  uint8_t type;
  uint16_t sequence;
  uint32_t timestampMs;
};

struct __attribute__((packed)) Frame {
  PacketHeader header;
  uint8_t payloadSize;
  uint8_t payload[MAX_PAYLOAD_SIZE];
};

}  // namespace app::espnow
