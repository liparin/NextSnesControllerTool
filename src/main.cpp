#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "config.h"
#include "xml_parser.h"
#include "http_client.h"
#include "usb_device.h"

#define IDC_URL_EDIT       101
#define IDC_LIST_BOX       102
#define IDC_BTN_LOAD       103
#define IDC_BTN_DOWNLOAD   104
#define IDC_BTN_APPLY      105
#define IDC_STATUS_LABEL   106
#define IDC_DEVICE_COMBO   107

DeviceConfig g_config;
std::vector<UsbDeviceInfo> g_devices;
HWND hWndMain;
HWND hEditUrl;
HWND hListBox;
HWND hStatusLabel;
HWND hDeviceCombo;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void UpdateListBox() {
    SendMessage(hListBox, LB_RESETCONTENT, 0, 0);
    for (const auto& btn : g_config.mappings) {
        std::wstring item = L"[" + std::wstring(btn.name.begin(), btn.name.end()) +
                            L"] -> " + std::wstring(btn.key.begin(), btn.key.end()) +
                            L" (" + std::wstring(btn.mode.begin(), btn.mode.end()) + L")";
        SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)item.c_str());
    }
}

void LogMessage(const wchar_t* msg) {
    SetWindowText(hStatusLabel, msg);
}

std::wstring StringToWString(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), NULL, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.length(), &wstr[0], len);
    return wstr;
}

std::string WStringToString(const std::wstring& ws) {
    if (ws.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(), NULL, 0, NULL, NULL);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.length(), &str[0], len, NULL, NULL);
    return str;
}

void OnBtnLoad() {
    wchar_t url[256];
    GetWindowText(hEditUrl, url, 256);
    if (!wcslen(url)) {
        LogMessage(L"Enter URL first.");
        return;
    }

    LogMessage(L"Downloading .lpl file...");

    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring destPath = std::wstring(tempPath) + L"next_snes_config.lpl";

    HttpClient http;
    std::wstring error;
    if (!http.download(url, destPath, error)) {
        LogMessage(error.c_str());
        return;
    }

    std::ifstream file(destPath, std::ios::binary);
    if (!file.is_open()) {
        LogMessage(L"Cannot open downloaded file.");
        return;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    XmlParser parser;
    g_config = DeviceConfig();
    if (!parser.parseFile(content, g_config)) {
        LogMessage(L"Failed to parse .lpl file.");
        return;
    }

    UpdateListBox();
    LogMessage(L"Config loaded. Device: %S");

    std::wstring status = L"Config loaded: " + StringToWString(g_config.device) +
                          L" v" + StringToWString(g_config.firmware_version);
    LogMessage(status.c_str());
}

void OnBtnDownload() {
    if (g_config.url.empty()) {
        LogMessage(L"Load .lpl config first.");
        return;
    }

    LogMessage(L"Downloading firmware...");

    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring destPath = std::wstring(tempPath) + L"next_snes_firmware.bin";

    std::wstring url = StringToWString(g_config.url);
    HttpClient http;
    std::wstring error;
    if (!http.download(url, destPath, error)) {
        LogMessage(error.c_str());
        return;
    }

    LogMessage(L"Firmware downloaded to temp folder.");
}

void OnBtnApply() {
    int sel = (int)SendMessage(hDeviceCombo, CB_GETCURSEL, 0, 0);
    if (sel == CB_ERR || sel >= (int)g_devices.size()) {
        LogMessage(L"Select a device first.");
        return;
    }

    LogMessage(L"Opening device...");

    UsbDevice usb;
    HANDLE hDevice = usb.openDevice(g_devices[sel].path);
    if (hDevice == INVALID_HANDLE_VALUE) {
        LogMessage(L"Cannot open device. Try another USB port.");
        return;
    }

    LogMessage(L"Sending mapping to controller...");

    unsigned char report[64];
    memset(report, 0, sizeof(report));

    report[0] = 0x01;
    report[1] = (unsigned char)g_config.mappings.size();

    for (size_t i = 0; i < g_config.mappings.size() && i < 30; i++) {
        report[2 + i * 2] = (unsigned char)(i + 1);
        report[3 + i * 2] = (unsigned char)g_config.mappings[i].key[0];
    }

    bool ok = usb.sendFeatureReport(hDevice, report, sizeof(report));
    usb.closeDevice(hDevice);

    if (ok) {
        LogMessage(L"Mapping applied successfully.");
    } else {
        LogMessage(L"Failed to send mapping. Wrong protocol?");
    }
}

void RefreshDeviceList() {
    SendMessage(hDeviceCombo, CB_RESETCONTENT, 0, 0);
    UsbDevice usb;
    g_devices = usb.enumerateHidDevices();
    for (const auto& dev : g_devices) {
        SendMessage(hDeviceCombo, CB_ADDSTRING, 0, (LPARAM)dev.name.c_str());
    }
    if (!g_devices.empty()) {
        SendMessage(hDeviceCombo, CB_SETCURSEL, 0, 0);
        LogMessage(L"Found devices. Select one.");
    } else {
        LogMessage(L"No HID devices found.");
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    const wchar_t CLASS_NAME[] = L"NextSNESControllerToolClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    hWndMain = CreateWindowEx(0, CLASS_NAME, L"Next SNES Controller Tool",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 520, 440,
        NULL, NULL, hInstance, NULL);

    if (!hWndMain) return 0;

    RECT rc;
    GetClientRect(hWndMain, &rc);
    int pad = 10;
    int rowH = 26;

    int y = pad;

    CreateWindow(L"STATIC", L"URL:", WS_CHILD | WS_VISIBLE | SS_RIGHT, pad, y + 3, 50, 20, hWndMain, NULL, hInstance, NULL);
    hEditUrl = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        pad + 60, y, rc.right - pad * 2 - 60, rowH, hWndMain, (HMENU)IDC_URL_EDIT, hInstance, NULL);
    y += rowH + 8;

    CreateWindow(L"BUTTON", L"Load Config", WS_CHILD | WS_VISIBLE, pad, y, 120, rowH, hWndMain, (HMENU)IDC_BTN_LOAD, hInstance, NULL);
    CreateWindow(L"BUTTON", L"Download Firmware", WS_CHILD | WS_VISIBLE, pad + 130, y, 150, rowH, hWndMain, (HMENU)IDC_BTN_DOWNLOAD, hInstance, NULL);
    y += rowH + 8;

    CreateWindow(L"STATIC", L"Device:", WS_CHILD | WS_VISIBLE | SS_RIGHT, pad, y + 3, 50, 20, hWndMain, NULL, hInstance, NULL);
    hDeviceCombo = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        pad + 60, y, rc.right - pad * 2 - 60, 200, hWndMain, (HMENU)IDC_DEVICE_COMBO, hInstance, NULL);
    y += rowH + 8;

    CreateWindow(L"STATIC", L"Button Mapping:", WS_CHILD | WS_VISIBLE, pad, y, 200, 20, hWndMain, NULL, hInstance, NULL);
    y += 24;

    hListBox = CreateWindow(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        pad, y, rc.right - pad * 2, rc.bottom - y - pad - rowH - 8 - 20, hWndMain, (HMENU)IDC_LIST_BOX, hInstance, NULL);

    int btnY = rc.bottom - pad - rowH - 20;
    CreateWindow(L"BUTTON", L"Apply to Device", WS_CHILD | WS_VISIBLE, pad, btnY, 150, rowH, hWndMain, (HMENU)IDC_BTN_APPLY, hInstance, NULL);

    hStatusLabel = CreateWindow(L"STATIC", L"Ready.", WS_CHILD | WS_VISIBLE | SS_LEFT,
        pad, rc.bottom - pad - 18, rc.right - pad * 2, 18, hWndMain, (HMENU)IDC_STATUS_LABEL, hInstance, NULL);

    RefreshDeviceList();

    ShowWindow(hWndMain, nShowCmd);
    UpdateWindow(hWndMain);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == IDC_BTN_LOAD) OnBtnLoad();
            else if (id == IDC_BTN_DOWNLOAD) OnBtnDownload();
            else if (id == IDC_BTN_APPLY) OnBtnApply();
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
