#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace coronet {

enum class PairingPhase : uint8_t {
    Inactive,
    WaitingForConfirmations,
    ReadyToDeliver,
    AwaitingReceipt,
    Completed,
    Cancelled,
    Expired,
};

struct PairingSnapshot {
    PairingPhase phase = PairingPhase::Inactive;
    uint32_t sessionId = 0;
    uint32_t code = 0;
    uint32_t expiresAtMs = 0;
    uint32_t revision = 0;
    bool phoneConfirmed = false;
    bool deviceConfirmed = false;
};

class PairingService {
public:
    static constexpr uint32_t SessionDurationMs = 120000U;

    void loop();
    PairingSnapshot beginPairing();
    bool confirmFromPhone(uint32_t sessionId, uint32_t code);
    bool confirmOnDevice(uint32_t sessionId);
    bool markResultSent(uint32_t sessionId);
    bool completeFromPhone(uint32_t sessionId);
    bool cancel(uint32_t sessionId = 0);
    void dismiss();
    PairingSnapshot snapshot() const;
    bool active() const;

private:
    void updateReadyStateLocked();
    bool sessionMatchesLocked(uint32_t sessionId) const;

    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
    PairingSnapshot state_;
};

PairingService& pairingService();

}
