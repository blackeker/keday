// tray.cpp – System tray icon and context menu management
#include "tray.h"
#include "app.h"
#include "resources.h"
#include "features.h"
#include <shellapi.h>

void TrayCreate(HWND hWnd, HINSTANCE hInst) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize          = sizeof(NOTIFYICONDATAW);
    nid.hWnd            = hWnd;
    nid.uID             = 1;
    nid.uFlags          = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY_ICON;
    nid.hIcon           = LoadIconW(hInst, MAKEINTRESOURCE(IDI_APP_ICON));
    wcscpy_s(nid.szTip, L"Keday");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayDestroy(HWND hWnd) {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd   = hWnd;
    nid.uID    = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

// Build and show the full tray icon right-click menu
void TrayShowMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    // Happiness indicator (grayed out, informational)
    std::wstring happStr = L"Mutluluk: " + std::to_wstring(g_happinessLevel) + L"%  |  " + g_catName;
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, happStr.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenuW(hMenu, MF_STRING, IDM_PET,  L"\U0001F43E  Sev (Pat)");
    AppendMenuW(hMenu, MF_STRING, IDM_FEED, L"\U0001F41F  Yem Ver");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    // Color theme sub-menu
    HMENU hTheme = CreatePopupMenu();
    AppendMenuW(hTheme, MF_STRING, IDM_THEME_NORMAL,   L"Normal");
    AppendMenuW(hTheme, MF_STRING, IDM_THEME_SEPIA,    L"Sepia (Retro)");
    AppendMenuW(hTheme, MF_STRING, IDM_THEME_NEON,     L"Neon");
    AppendMenuW(hTheme, MF_STRING, IDM_THEME_HOLOGRAM, L"Hologram");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTheme, L"\U0001F3A8  Renk Temas\u0131");

    // Toys sub-menu
    HMENU hToys = CreatePopupMenu();
    AppendMenuW(hToys, MF_STRING, IDM_TOY_YARN,  L"\U0001F9F6  \u0130p Yuma\u011f\u0131");
    AppendMenuW(hToys, MF_STRING, IDM_TOY_LASER, L"\U0001F534  Lazer Dot");
    AppendMenuW(hToys, MF_STRING, IDM_TOY_MOUSE, L"\U0001F401  Oyuncak Fare");
    AppendMenuW(hToys, MF_STRING, IDM_TOY_WATER, L"\U0001F4A7  Su Kab\u0131");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hToys, L"\U0001F9F8  Oyuncak B\u0131rak");

    AppendMenuW(hMenu, MF_STRING, IDM_RENAME, L"\U0001F4DD  \u0130sim De\u011fi\u015ftir");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"\u2699\uFE0F  Ayarlar");
    AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_VISIBLE,
                g_bVisible ? L"\U0001F648  Kediyi Gizle" : L"\U0001F63A  Kediyi G\u00f6ster");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"\u274C  \u00c7\u0131k\u0131\u015f");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}

// Compact menu when right-clicking the cat directly
void TrayShowCatMenu(HWND hWnd) {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    std::wstring happStr = L"Mutluluk: " + std::to_wstring(g_happinessLevel) + L"%";
    AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, happStr.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_PET,      L"\U0001F43E  Sev");
    AppendMenuW(hMenu, MF_STRING, IDM_FEED,     L"\U0001F41F  Yem Ver");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"\u2699\uFE0F  Ayarlar");
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,     L"\u274C  \u00c7\u0131k\u0131\u015f");

    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(hMenu);
}
