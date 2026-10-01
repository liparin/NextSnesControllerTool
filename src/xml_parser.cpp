#include "xml_parser.h"
#include <algorithm>

std::string XmlParser::extractTag(const std::string& xml, const std::string& tag) {
    std::string open = "<" + tag + ">";
    std::string close = "</" + tag + ">";
    size_t start = xml.find(open);
    if (start == std::string::npos) return "";
    start += open.length();
    size_t end = xml.find(close, start);
    if (end == std::string::npos) return "";
    return xml.substr(start, end - start);
}

std::string XmlParser::extractAttribute(const std::string& xml, const std::string& attr) {
    std::string search = attr + "=\"";
    size_t start = xml.find(search);
    if (start == std::string::npos) return "";
    start += search.length();
    size_t end = xml.find("\"", start);
    if (end == std::string::npos) return "";
    return xml.substr(start, end - start);
}

void XmlParser::parseMappings(const std::string& xml, DeviceConfig& config) {
    size_t pos = 0;
    while (true) {
        size_t btnStart = xml.find("<button ", pos);
        if (btnStart == std::string::npos) break;
        size_t btnEnd = xml.find("</button>", btnStart);
        if (btnEnd == std::string::npos) break;

        std::string btnBlock = xml.substr(btnStart, btnEnd - btnStart + 9);

        ButtonMapping mapping;
        mapping.name = extractAttribute(btnBlock, "name");
        mapping.key = extractTag(btnBlock, "key");
        mapping.mode = extractTag(btnBlock, "mode");

        config.mappings.push_back(mapping);
        pos = btnEnd + 9;
    }
}

bool XmlParser::parseFile(const std::string& content, DeviceConfig& config) {
    config.device = extractTag(content, "device");
    config.firmware_version = extractTag(content, "firmware_version");
    config.url = extractTag(content, "url");
    config.sha256 = extractTag(content, "sha256");

    std::string archiveStr = extractTag(content, "archive");
    config.archive = (archiveStr == "true" || archiveStr == "1");

    parseMappings(content, config);

    return !config.device.empty();
}
