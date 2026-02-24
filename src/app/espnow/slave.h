#pragma once

#include <Arduino.h>
#include <esp_now.h>

#include "protocol.h"

namespace app::espnow {

class SlaveNode {
 public:
  SlaveNode() = default;

  bool begin(uint8_t channel = 1);
  void loop();

  bool sendState(const char* text);
  bool sendStateBinary(const void* payload, size_t payloadSize);
  bool isReady() const { return started; }
  bool isMasterLinked() const { return masterKnown; }

 private:
  static void onSendStatic(const esp_now_send_info_t* tx_info, esp_now_send_status_t status);
  static void onReceiveStatic(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len);

    bool matchesMasterBeacon(const uint8_t* payload, uint8_t payloadSize) const;
    void scanNextChannel();
  bool addMasterPeer(const uint8_t mac[6]);
  bool sendToMaster(PacketType type, const void* payload, size_t payloadSize);

  static SlaveNode* activeInstance;

  uint16_t sequence = 0;
  bool started = false;
  bool masterKnown = false;
  uint8_t masterMac[6] = {0};
  uint8_t scanChannel = DEFAULT_CHANNEL;

  uint32_t lastHelloMs = 0;
  uint32_t lastScanMs = 0;
  uint32_t lastMasterSeenMs = 0;
};

extern SlaveNode espnowSlave;

}  // namespace app::espnow
