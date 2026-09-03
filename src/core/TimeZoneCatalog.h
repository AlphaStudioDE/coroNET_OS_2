#pragma once

#include <stddef.h>
#include <string.h>

namespace coronet {

struct TimeZoneOption {
    const char* label;
    const char* offset;
    const char* spec;
};

static constexpr TimeZoneOption TimeZoneOptions[] = {
    {"APIA / NUKU'ALOFA", "UTC+13", "<+13>-13"},
    {"AUCKLAND", "UTC+12/+13", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"FIJI", "UTC+12", "<+12>-12"},
    {"HONIARA", "UTC+11", "<+11>-11"},
    {"SYDNEY / MELBOURNE", "UTC+10/+11", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"ADELAIDE", "UTC+9:30/+10:30", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
    {"DARWIN", "UTC+9:30", "ACST-9:30"},
    {"TOKYO / SEOUL", "UTC+9", "JST-9"},
    {"SHANGHAI / SINGAPORE", "UTC+8", "CST-8"},
    {"BANGKOK / JAKARTA", "UTC+7", "<+07>-7"},
    {"YANGON", "UTC+6:30", "<+0630>-6:30"},
    {"DHAKA", "UTC+6", "<+06>-6"},
    {"KOLKATA / MUMBAI", "UTC+5:30", "IST-5:30"},
    {"ALMATY / KARACHI / TASHKENT", "UTC+5", "<+05>-5"},
    {"DUBAI / MUSCAT", "UTC+4", "<+04>-4"},
    {"MOSCOW / MINSK", "UTC+3", "MSK-3"},
    {"CAIRO", "UTC+2/+3", "EET-2EEST,M4.5.5/0,M10.5.4/24"},
    {"HELSINKI / ATHENS / KYIV", "UTC+2/+3", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"JOHANNESBURG", "UTC+2", "SAST-2"},
    {"BERLIN / WARSAW", "UTC+1/+2", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"LONDON / DUBLIN", "UTC+0/+1", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"UTC / REYKJAVIK", "UTC+0", "UTC0"},
    {"AZORES", "UTC-1/+0", "<-01>1<+00>,M3.5.0/0,M10.5.0/1"},
    {"SAO PAULO", "UTC-3", "BRT3"},
    {"BUENOS AIRES", "UTC-3", "<-03>3"},
    {"NEW YORK / TORONTO", "UTC-5/-4", "EST5EDT,M3.2.0,M11.1.0"},
    {"CHICAGO", "UTC-6/-5", "CST6CDT,M3.2.0,M11.1.0"},
    {"MEXICO CITY", "UTC-6", "CST6"},
    {"DENVER", "UTC-7/-6", "MST7MDT,M3.2.0,M11.1.0"},
    {"PHOENIX", "UTC-7", "MST7"},
    {"LOS ANGELES / VANCOUVER", "UTC-8/-7", "PST8PDT,M3.2.0,M11.1.0"},
    {"ANCHORAGE / JUNEAU", "UTC-9/-8", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"HONOLULU / HAWAII", "UTC-10", "HST10"},
    {"PAGO PAGO / MIDWAY", "UTC-11", "<-11>11"},
};

constexpr size_t TimeZoneOptionCount = sizeof(TimeZoneOptions) / sizeof(TimeZoneOptions[0]);

inline int timeZoneOptionIndex(const char* spec) {
    if (!spec) return -1;
    for (size_t index = 0; index < TimeZoneOptionCount; ++index) {
        if (strcmp(TimeZoneOptions[index].spec, spec) == 0) return static_cast<int>(index);
    }
    return -1;
}

inline const char* timeZoneOptionLabel(const char* spec) {
    const int index = timeZoneOptionIndex(spec);
    return index >= 0 ? TimeZoneOptions[index].label : "CUSTOM";
}

}
