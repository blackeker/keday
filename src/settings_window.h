#ifndef SETTINGS_WINDOW_H
#define SETTINGS_WINDOW_H

#include <windows.h>
#include "settings.h"

void OpenSettingsWindow(HINSTANCE hInstance, HWND hParent, Settings& currentSettings, void (*onSettingsChanged)());
bool IsSettingsWindowOpen();
void CloseSettingsWindow();

#endif // SETTINGS_WINDOW_H
