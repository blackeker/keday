#include "settings.h"
#include <shlobj.h>
#include <shlwapi.h>

std::wstring GetSettingsPath() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        PathAppendW(path, L"Keday");
        CreateDirectoryW(path, NULL);
        PathAppendW(path, L"settings.ini");
        return path;
    }
    return L"settings.ini";
}

void LoadSettings(Settings& settings) {
    std::wstring path = GetSettingsPath();
    settings.speed = GetPrivateProfileIntW(L"Settings", L"speed", 10, path.c_str());
    settings.size = GetPrivateProfileIntW(L"Settings", L"size", 100, path.c_str());
    settings.alwaysOnTop = GetPrivateProfileIntW(L"Settings", L"alwaysOnTop", 1, path.c_str()) != 0;
    settings.minimizeToTray = GetPrivateProfileIntW(L"Settings", L"minimizeToTray", 1, path.c_str()) != 0;
    settings.autoLaunch = GetPrivateProfileIntW(L"Settings", L"autoLaunch", 1, path.c_str()) != 0;
    settings.hideOnFullscreen = GetPrivateProfileIntW(L"Settings", L"hideOnFullscreen", 1, path.c_str()) != 0;
    
    wchar_t buf[256];
    GetPrivateProfileStringW(L"Settings", L"character", L"oneko", buf, 256, path.c_str());
    settings.character = buf;
    
    settings.showShadow = GetPrivateProfileIntW(L"Settings", L"showShadow", 0, path.c_str()) != 0;
    settings.opacity = GetPrivateProfileIntW(L"Settings", L"opacity", 100, path.c_str());
    settings.followMouse = GetPrivateProfileIntW(L"Settings", L"followMouse", 1, path.c_str()) != 0;
}

void SaveSettings(const Settings& settings) {
    std::wstring path = GetSettingsPath();
    WritePrivateProfileStringW(L"Settings", L"speed", std::to_wstring(settings.speed).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"size", std::to_wstring(settings.size).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"alwaysOnTop", std::to_wstring(settings.alwaysOnTop ? 1 : 0).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"minimizeToTray", std::to_wstring(settings.minimizeToTray ? 1 : 0).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"autoLaunch", std::to_wstring(settings.autoLaunch ? 1 : 0).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"hideOnFullscreen", std::to_wstring(settings.hideOnFullscreen ? 1 : 0).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"character", settings.character.c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"showShadow", std::to_wstring(settings.showShadow ? 1 : 0).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"opacity", std::to_wstring(settings.opacity).c_str(), path.c_str());
    WritePrivateProfileStringW(L"Settings", L"followMouse", std::to_wstring(settings.followMouse ? 1 : 0).c_str(), path.c_str());
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    LONG lResult = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey);
    if (lResult == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(NULL, exePath, MAX_PATH);
            std::wstring val = L"\"" + std::wstring(exePath) + L"\"";
            RegSetValueExW(hKey, L"Keday", 0, REG_SZ, (BYTE*)val.c_str(), (val.length() + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"Keday");
        }
        RegCloseKey(hKey);
    }
}
