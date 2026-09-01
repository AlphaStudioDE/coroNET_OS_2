#pragma once

#include <cstddef>

namespace coronet {

class BleService {
public:
    void begin();
    void loop();
    void publishEvent(const char* type, const char* message);
    void queueCommand(const char* command, size_t length);

private:
    bool started_ = false;
    bool connected_ = false;
    bool stateDirty_ = true;
    unsigned long lastNotifyMs_ = 0;
    unsigned long revision_ = 0;
    char pendingCommand_[192] = "";
    bool commandPending_ = false;
    char deviceId_[13] = "";
    char advertisedName_[25] = "";

    void handleCommand(const char* command);
    void publishState(bool force);
    void publishSettings();
    void refreshAdvertisedName();
};

}
