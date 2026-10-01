#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <string>
#include <vector>
#include <windows.h>

struct UsbDeviceInfo {
    std::wstring path;
    std::wstring name;
    unsigned short vendorId;
    unsigned short productId;
};

class UsbDevice {
public:
    std::vector<UsbDeviceInfo> enumerateHidDevices();
    HANDLE openDevice(const std::wstring& path);
    bool sendFeatureReport(HANDLE hDevice, const unsigned char* data, size_t length);
    bool readFeatureReport(HANDLE hDevice, unsigned char* data, size_t length);
    bool sendOutputReport(HANDLE hDevice, const unsigned char* data, size_t length);
    void closeDevice(HANDLE hDevice);

private:
    bool getHidAttributes(HANDLE hDevice, unsigned short& vid, unsigned short& pid);
};

#endif
