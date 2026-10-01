#include "usb_device.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <devguid.h>
#include <cstring>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

std::vector<UsbDeviceInfo> UsbDevice::enumerateHidDevices() {
    std::vector<UsbDeviceInfo> devices;

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return devices;

    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, i, &interfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(hDevInfo, &interfaceData, NULL, 0, &requiredSize, NULL);

        if (requiredSize == 0) continue;

        std::vector<unsigned char> detailBuffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)detailBuffer.data();
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(hDevInfo, &interfaceData, detailData, requiredSize, NULL, NULL)) {
            continue;
        }

        std::wstring devicePath = detailData->DevicePath;

        HANDLE hDevice = CreateFileW(devicePath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDevice == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attributes;
        attributes.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(hDevice, &attributes)) {
            UsbDeviceInfo info;
            info.path = devicePath;
            info.vendorId = attributes.VendorID;
            info.productId = attributes.ProductID;
            info.name = L"HID Device VID_" + std::to_wstring(attributes.VendorID) +
                        L" PID_" + std::to_wstring(attributes.ProductID);
            devices.push_back(info);
        }

        CloseHandle(hDevice);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return devices;
}

HANDLE UsbDevice::openDevice(const std::wstring& path) {
    HANDLE hDevice = CreateFileW(path.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);
    return hDevice;
}

bool UsbDevice::sendOutputReport(HANDLE hDevice, const unsigned char* data, size_t length) {
    if (hDevice == INVALID_HANDLE_VALUE) return false;

    std::vector<unsigned char> report(length + 1);
    report[0] = 0;
    memcpy(report.data() + 1, data, length);

    DWORD bytesWritten = 0;
    bool result = WriteFile(hDevice, report.data(), (DWORD)report.size(), &bytesWritten, NULL);
    return result && bytesWritten == report.size();
}

bool UsbDevice::sendFeatureReport(HANDLE hDevice, const unsigned char* data, size_t length) {
    if (hDevice == INVALID_HANDLE_VALUE) return false;

    std::vector<unsigned char> report(length + 1);
    report[0] = 0;
    memcpy(report.data() + 1, data, length);

    return HidD_SetFeature(hDevice, report.data(), (ULONG)report.size());
}

bool UsbDevice::readFeatureReport(HANDLE hDevice, unsigned char* data, size_t length) {
    if (hDevice == INVALID_HANDLE_VALUE) return false;

    std::vector<unsigned char> report(length + 1, 0);
    report[0] = 0;

    if (!HidD_GetFeature(hDevice, report.data(), (ULONG)report.size())) return false;

    memcpy(data, report.data() + 1, length);
    return true;
}

void UsbDevice::closeDevice(HANDLE hDevice) {
    if (hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(hDevice);
    }
}
