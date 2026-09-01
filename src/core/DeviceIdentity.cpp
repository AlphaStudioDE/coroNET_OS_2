#include "DeviceIdentity.h"

#include <Arduino.h>

#include "../config/AppConfig.h"

namespace coronet {

namespace {
DeviceIdentity gDeviceIdentity;
}

DeviceIdentity& deviceIdentity() {
    return gDeviceIdentity;
}

const char* DeviceIdentity::id() {
    ensure();
    return id_;
}

const char* DeviceIdentity::defaultName() {
    ensure();
    return defaultName_;
}

const char* DeviceIdentity::hostname() {
    ensure();
    return hostname_;
}

void DeviceIdentity::effectiveName(const char* customName, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    ensure();

    char clean[25] = "";
    sanitizeName(customName, clean, sizeof(clean));
    strlcpy(out, clean[0] ? clean : defaultName_, outSize);
}

void DeviceIdentity::sanitizeName(const char* input, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!input) return;

    size_t n = 0;
    while (*input && n + 1 < outSize) {
        const char c = *input++;
        const bool ok = (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') ||
                        c == ' ' || c == '_' || c == '-';
        if (ok) out[n++] = c;
    }

    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = '\0';
}

void DeviceIdentity::ensure() {
    if (initialized_) return;

    const uint64_t mac = ESP.getEfuseMac();
    snprintf(id_, sizeof(id_), "%012llX", static_cast<unsigned long long>(mac & 0xFFFFFFFFFFFFULL));

    const char* suffix = strlen(id_) >= 4 ? id_ + strlen(id_) - 4 : "0000";
    snprintf(defaultName_, sizeof(defaultName_), "%s_%s", config::AppName, suffix);
    snprintf(hostname_, sizeof(hostname_), "coronet-%s", suffix);
    for (char* p = hostname_; *p; ++p) {
        if (*p >= 'A' && *p <= 'Z') *p = static_cast<char>(*p - 'A' + 'a');
    }

    initialized_ = true;
}

}
