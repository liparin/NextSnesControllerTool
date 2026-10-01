#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>

class HttpClient {
public:
    bool download(const std::wstring& url, const std::wstring& destPath, std::wstring& error);

private:
    bool parseUrl(const std::wstring& url, std::wstring& host, std::wstring& path, int& port, bool& https);
};

#endif
