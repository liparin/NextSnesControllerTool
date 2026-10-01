#ifndef XML_PARSER_H
#define XML_PARSER_H

#include <string>
#include "config.h"

class XmlParser {
public:
    bool parseFile(const std::string& content, DeviceConfig& config);

private:
    std::string extractTag(const std::string& xml, const std::string& tag);
    std::string extractAttribute(const std::string& xml, const std::string& attr);
    void parseMappings(const std::string& xml, DeviceConfig& config);
};

#endif
