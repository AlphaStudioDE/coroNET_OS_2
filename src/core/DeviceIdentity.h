#pragma once

#include <cstddef>

namespace coronet {

class DeviceIdentity {
public:
    const char* id();
    const char* defaultName();
    const char* hostname();
    void effectiveName(const char* customName, char* out, size_t outSize);
    void sanitizeName(const char* input, char* out, size_t outSize);

private:
    bool initialized_ = false;
    char id_[13] = "";
    char defaultName_[25] = "";
    char hostname_[25] = "";

    void ensure();
};

DeviceIdentity& deviceIdentity();

}
