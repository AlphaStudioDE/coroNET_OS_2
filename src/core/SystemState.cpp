#include "SystemState.h"

namespace coronet {

static SystemState gState;

SystemState& state() {
    return gState;
}

}
