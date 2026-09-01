#include "AudioService.h"

#include <Arduino.h>

#include "../core/SystemState.h"

namespace coronet {

void AudioService::begin() {
    state().audioReady = false;
    Serial.println("[audio] hardware initialization pending");
}

void AudioService::loop() {
}

}
