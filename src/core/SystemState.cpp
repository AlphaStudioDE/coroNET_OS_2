#include "SystemState.h"

namespace coronet {

static SystemState gState;
static portMUX_TYPE gStateMux = portMUX_INITIALIZER_UNLOCKED;

SystemState& state() {
    return gState;
}

portMUX_TYPE& stateMutex() {
    return gStateMux;
}

SystemState stateSnapshot() {
    SystemState copy;
    portENTER_CRITICAL(&gStateMux);
    copy = gState;
    portEXIT_CRITICAL(&gStateMux);
    return copy;
}

}
