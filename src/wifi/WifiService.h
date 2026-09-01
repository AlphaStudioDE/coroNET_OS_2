#pragma once

namespace coronet {

class WifiService {
public:
    void begin();
    void loop();

private:
    char activeSsid_[33] = "";
    char activePassword_[65] = "";
    bool started_ = false;

    void applySettings();
};

}
