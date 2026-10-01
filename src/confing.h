#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>

struct ButtonMapping {
    std::string name;
    std::string key;
    std::string mode;
};

struct DeviceConfig {
    std::string device;
    std::string firmware_version;
    std::string url;
    std::string sha256;
    bool archive;
    std::vector<ButtonMapping> mappings;
};

#endif
