#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

struct Settings {
    int speed;
    int size;
    bool alwaysOnTop;
    bool minimizeToTray;
    bool autoLaunch;
    bool hideOnFullscreen;
    std::wstring character;
    bool showShadow;
    int opacity;
    bool followMouse;
    bool isDarkMode;
    
    // Advanced features properties
    int version;
    int volume;
    bool clickThrough;
    int accessory; // 0: None, 1: Glasses, 2: Santa Hat, 3: Bow Tie
    bool musicMode;
};

void LoadSettings(Settings& settings);
void SaveSettings(const Settings& settings);
void SetAutoStart(bool enable);

#endif // SETTINGS_H
