// tray.h – System tray icon and context menu management
#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

// Call from WM_CREATE to register the tray icon
void TrayCreate(HWND hWnd, HINSTANCE hInst);

// Call from WM_DESTROY
void TrayDestroy(HWND hWnd);

// Show the right-click tray context menu
void TrayShowMenu(HWND hWnd);

// Show the compact right-click menu (from clicking directly on the cat)
void TrayShowCatMenu(HWND hWnd);

#endif // TRAY_H
