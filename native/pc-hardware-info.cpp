#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_4.h>
#include <cstdio>
#include <string>

static std::string JsonString(const wchar_t* value) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "\"Unknown GPU\"";
    std::string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, utf8.data(), size, nullptr, nullptr);
    utf8.pop_back();
    std::string result = "\"";
    for (unsigned char c : utf8) {
        if (c == '\"' || c == '\\') { result += '\\'; result += c; }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof(escaped), "\\u%04x", c); result += escaped; }
        else result += c;
    }
    return result + '\"';
}

int main() {
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return 1;
    std::printf("{\"adapters\":[");
    bool first = true;
    for (UINT index = 0; index < 32; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) == DXGI_ERROR_NOT_FOUND) break;
        if (!adapter) continue;
        DXGI_ADAPTER_DESC1 desc{};
        if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            if (!first) std::printf(",");
            first = false;
            const auto name = JsonString(desc.Description);
            // SIZE_T preserves VRAM sizes above 4 GiB; WMI AdapterRAM does not.
            std::printf("{\"name\":%s,\"dedicatedBytes\":%llu,\"sharedBytes\":%llu,\"vendorId\":%u,\"deviceId\":%u}",
                        name.c_str(), static_cast<unsigned long long>(desc.DedicatedVideoMemory),
                        static_cast<unsigned long long>(desc.SharedSystemMemory), desc.VendorId, desc.DeviceId);
        }
        adapter->Release();
    }
    std::printf("]}\n");
    factory->Release();
    return 0;
}
