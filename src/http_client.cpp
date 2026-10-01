#include "http_client.h"
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

bool HttpClient::parseUrl(const std::wstring& url, std::wstring& host, std::wstring& path, int& port, bool& https) {
    https = false;
    port = 80;

    size_t schemeEnd = std::wstring::npos;
    if (url.compare(0, 8, L"https://") == 0) {
        https = true;
        port = 443;
        schemeEnd = 8;
    } else if (url.compare(0, 7, L"http://") == 0) {
        schemeEnd = 7;
    } else {
        return false;
    }

    size_t hostEnd = url.find(L"/", schemeEnd);
    if (hostEnd == std::wstring::npos) {
        host = url.substr(schemeEnd);
        path = L"/";
    } else {
        host = url.substr(schemeEnd, hostEnd - schemeEnd);
        path = url.substr(hostEnd);
    }

    size_t colonPos = host.find(L":");
    if (colonPos != std::wstring::npos) {
        std::wstring portStr = host.substr(colonPos + 1);
        port = _wtoi(portStr.c_str());
        host = host.substr(0, colonPos);
    }

    return !host.empty();
}

bool HttpClient::download(const std::wstring& url, const std::wstring& destPath, std::wstring& error) {
    std::wstring host, path;
    int port;
    bool https;

    if (!parseUrl(url, host, path, port, https)) {
        error = L"Invalid URL";
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"NextSNESTool/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { error = L"WinHttpOpen failed"; return false; }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) { error = L"WinHttpConnect failed"; WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { error = L"WinHttpOpenRequest failed"; WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        error = L"WinHttpSendRequest failed";
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        error = L"WinHttpReceiveResponse failed";
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        error = L"Cannot create file";
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD bytesRead = 0;
    char buffer[8192];
    bool success = true;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        DWORD written = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &written, NULL) || written != bytesRead) {
            error = L"WriteFile failed";
            success = false;
            break;
        }
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return success;
}
