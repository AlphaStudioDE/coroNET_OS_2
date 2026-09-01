#pragma once

namespace coronet {

class SystemHealth {
public:
    void begin();
    void loop();
    void sample();
    void log() const;

private:
    unsigned long lastSampleMs_ = 0;
    unsigned long lastLogMs_ = 0;
};

}
