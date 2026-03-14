#pragma once

#include <Arduino.h>

namespace app::tasks {

bool startNetworkTask();

// Publish an outgoing payload to be sent by the network task.
bool publishOutgoingBinary(const void* payload, size_t payloadSize);
bool publishOutgoingText(const String& text);

}  // namespace app::tasks
