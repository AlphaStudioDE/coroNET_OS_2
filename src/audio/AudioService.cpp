#include "AudioService.h"

#include "../core/SystemState.h"

namespace coronet {

void AudioService::begin() {
    state().audioReady = true;
}

void AudioService::loop() {
}

}
