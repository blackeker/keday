// main.cpp – Application entry point and message dispatch for Keday
// All heavy logic lives in: neko.cpp, render.cpp, audio.cpp, tray.cpp, features.cpp
#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include "app.h"
#include "neko.h"
#include "render.h"
#include "audio.h"
#include "tray.h"
#include "features.h"
#include "settings.h"
#include "settings_window.h"
#include "resources.h"
#include <ctime>
#include <cmath>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

// ── Simple Input Dialog for Rename ────────────────────────────
static HWND g_hInputDlg = NULL;
static wchar_t g_inputBuffer[64] = {};
static bool g_inputOk = false;

static LRESULT CALLBACK InputDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HWND hEdit = CreateWindowExW(0, L"EDIT", g_catName.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            20, 20, 240, 28, hWnd, (HMENU)101, NULL, NULL);
        SendMessageW(hEdit, EM_SETLIMITTEXT, 30, 0);
        SetFocus(hEdit);
        SendMessageW(hEdit, EM_SETSEL, 0, -1);

        CreateWindowExW(0, L"BUTTON", L"Tamam",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            60, 60, 90, 30, hWnd, (HMENU)IDOK, NULL, NULL);
        CreateWindowExW(0, L"BUTTON", L"\u0130ptal",
            WS_CHILD | WS_VISIBLE,
            160, 60, 90, 30, hWnd, (HMENU)IDCANCEL, NULL, NULL);

        HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
            0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SendMessageW(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        EnumChildWindows(hWnd, [](HWND h, LPARAM lp) -> BOOL {
            SendMessageW(h, WM_SETFONT, (WPARAM)lp, TRUE); return TRUE;
        }, (LPARAM)hFont);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            HWND hEdit = GetDlgItem(hWnd, 101);
            GetWindowTextW(hEdit, g_inputBuffer, 64);
            g_inputOk = true;
            DestroyWindow(hWnd);
        } else if (LOWORD(wParam) == IDCANCEL) {
            g_inputOk = false;
            DestroyWindow(hWnd);
        }
        break;
    case WM_DESTROY:
        g_hInputDlg = NULL;
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

static void ShowRenameDialog(HINSTANCE hInst) {
    if (g_hInputDlg) { SetForegroundWindow(g_hInputDlg); return; }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = InputDlgProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"KedayInputClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w = 300, h = 130;

    g_inputOk = false;
    g_hInputDlg = CreateWindowExW(WS_EX_TOPMOST, L"KedayInputClass",
        L"Kedi \u0130smi De\u011fi\u015ftir",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (sw - w) / 2, (sh - h) / 2, w, h,
        NULL, NULL, hInst, NULL);
    ShowWindow(g_hInputDlg, SW_SHOW);
    UpdateWindow(g_hInputDlg);

    MSG msg;
    while (g_hInputDlg && GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(g_hInputDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_inputOk && wcslen(g_inputBuffer) > 0) {
        g_catName = g_inputBuffer;
    }
}

// ── WndProc ───────────────────────────────────────────────────
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    // ── Pixel-perfect hit testing ─────────────────────────────
    case WM_NCHITTEST: {
        if (g_settings.clickThrough && !(GetAsyncKeyState(VK_CONTROL) & 0x8000))
            return HTTRANSPARENT;

        POINT ptMouse = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT ptClient = ptMouse;
        ScreenToClient(hWnd, &ptClient);

        int catSize  = 32 * g_settings.size / 100;
        int extraTop = 90, extraSide = 80;
        int catL = extraSide, catR = extraSide + catSize;
        int catT = extraTop,  catB = extraTop  + catSize;

        if (ptClient.x >= catL && ptClient.x < catR &&
            ptClient.y >= catT && ptClient.y < catB) {
            Frame  frame = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
            int    fi    = frame.row * 8 + frame.col;
            if (fi >= 0 && fi < 48 && g_pSpriteFrames[fi] &&
                g_pSpriteFrames[fi]->GetLastStatus() == Ok) {
                Bitmap* bmp = g_pSpriteFrames[fi];
                int px = (ptClient.x - catL) * bmp->GetWidth()  / catSize;
                int py = (ptClient.y - catT) * bmp->GetHeight() / catSize;
                if (px >= 0 && px < (int)bmp->GetWidth() &&
                    py >= 0 && py < (int)bmp->GetHeight()) {
                    Color c; bmp->GetPixel(px, py, &c);
                    if (c.GetAlpha() > 20) return HTCLIENT;
                }
            }
        }
        return HTTRANSPARENT;
    }

    // ── Forward mousewheel to window underneath ───────────────
    case WM_MOUSEWHEEL: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        EnableWindow(hWnd, FALSE);
        HWND under = WindowFromPoint(pt);
        EnableWindow(hWnd, TRUE);
        if (under && under != hWnd) PostMessageW(under, WM_MOUSEWHEEL, wParam, lParam);
        break;
    }

    // ── Double-click: mega happiness burst ───────────────────
    case WM_LBUTTONDBLCLK: {
        g_happinessLevel = std::min(100, g_happinessLevel + 15);
        PlayMeowAsync();
        int sz = 32 * g_settings.size / 100;
        SpawnHearts(sz / 2.0, 25.0, 10);
        g_currentState      = STATE_ALERT;
        g_currentFrameIndex = 0;
        g_idleTicks         = 0;
        UpdateNekoWindow();
        break;
    }

    // ── Left button down: start drag ─────────────────────────
    case WM_LBUTTONDOWN: {
        PlayMeowAsync();
        OnMouseClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        g_isDragging   = true;
        g_isFalling    = false;
        g_velocityX    = 0.0;
        g_velocityY    = 0.0;
        SetCapture(hWnd);
        GetCursorPos(&g_dragStartMousePos);
        g_lastMousePos = g_dragStartMousePos;
        RECT rc; GetWindowRect(hWnd, &rc);
        g_dragStartWndPos = { rc.left, rc.top };
        g_currentState      = STATE_ALERT;
        g_currentFrameIndex = 0;
        UpdateNekoWindow();
        break;
    }

    // ── Mouse move: update drag position ─────────────────────
    case WM_MOUSEMOVE: {
        if (g_isDragging) {
            POINT cur; GetCursorPos(&cur);
            g_nekoX = g_dragStartWndPos.x + (cur.x - g_dragStartMousePos.x);
            g_nekoY = g_dragStartWndPos.y + (cur.y - g_dragStartMousePos.y);
            SetWindowPos(hWnd, NULL, (int)g_nekoX, (int)g_nekoY, 0, 0,
                         SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            UpdateNekoWindow();
        }
        break;
    }

    // ── Left button up: release drag, apply throw velocity ───
    case WM_LBUTTONUP: {
        if (g_isDragging) {
            g_isDragging = false;
            ReleaseCapture();
            g_isFalling = true;
            if (std::abs(g_velocityX) < 1.0 && std::abs(g_velocityY) < 1.0) {
                g_velocityX = 0.0;
                g_velocityY = 1.0;
            }
        }
        break;
    }

    // ── Right-click on cat: compact menu ─────────────────────
    case WM_RBUTTONUP:
        TrayShowCatMenu(hWnd);
        break;

    // ── Timer tick ───────────────────────────────────────────
    case WM_TIMER:
        if (UpdateNekoLogic()) UpdateNekoWindow();
        break;

    // ── Tray icon messages ────────────────────────────────────
    case WM_TRAY_ICON:
        if (lParam == WM_RBUTTONUP)      TrayShowMenu(hWnd);
        else if (lParam == WM_LBUTTONUP) PostMessageW(hWnd, WM_COMMAND, IDM_SETTINGS, 0);
        break;

    // ── Command dispatch ──────────────────────────────────────
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_SETTINGS:
            OpenSettingsWindow(g_hInstance, hWnd, g_settings, []() {
                ApplyWindowSettings();
                UpdateNekoWindow();
            });
            break;

        case IDM_TOGGLE_VISIBLE:
            g_bVisible = !g_bVisible;
            if (g_bVisible) { ShowWindow(hWnd, SW_SHOWNOACTIVATE); UpdateNekoWindow(); }
            else              ShowWindow(hWnd, SW_HIDE);
            break;

        case IDM_PET: {
            PlayPurrAsync();
            int sz = 32 * g_settings.size / 100;
            SpawnHearts(sz / 2.0, 25.0, 8);
            g_currentState      = STATE_SCRATCH_SELF;
            g_currentFrameIndex = 0;
            UpdateNekoWindow();
            break;
        }
        case IDM_FEED: {
            if (!g_isFeeding) {
                g_isFeeding   = true;
                int sz        = 32 * g_settings.size / 100;
                g_fishX       = sz / 2.0;
                g_fishY       = -20.0;
                g_currentState      = STATE_EXPECT_FOOD;
                g_currentFrameIndex = 0;
                UpdateNekoWindow();
            }
            break;
        }
        case IDM_THEME_NORMAL:    g_colorTheme = 0; ProgressQuest(L"theme_change"); UpdateNekoWindow(); break;
        case IDM_THEME_SEPIA:     g_colorTheme = 1; ProgressQuest(L"theme_change"); UpdateNekoWindow(); break;
        case IDM_THEME_NEON:      g_colorTheme = 2; ProgressQuest(L"theme_change"); UpdateNekoWindow(); break;
        case IDM_THEME_HOLOGRAM:  g_colorTheme = 3; ProgressQuest(L"theme_change"); UpdateNekoWindow(); break;

        case IDM_TOY_YARN:  SpawnToy(TOY_BALL_OF_YARN, (int)g_nekoX + 70, (int)g_nekoY + 30); UpdateNekoWindow(); break;
        case IDM_TOY_LASER: SpawnToy(TOY_LASER_DOT,    (int)g_nekoX + 70, (int)g_nekoY + 30); UpdateNekoWindow(); break;
        case IDM_TOY_MOUSE: SpawnToy(TOY_MOUSE_TOY,    (int)g_nekoX + 70, (int)g_nekoY + 30); UpdateNekoWindow(); break;
        case IDM_TOY_WATER: SpawnToy(TOY_WATER_BOWL,   (int)g_nekoX + 70, (int)g_nekoY + 30); UpdateNekoWindow(); break;

        case IDM_RENAME:
            ShowRenameDialog(g_hInstance);
            UpdateNekoWindow();
            break;
        case IDM_EXIT:
            CloseSettingsWindow();
            DestroyWindow(hWnd);
            break;
        }
        break;

    // ── Window creation ───────────────────────────────────────
    case WM_CREATE:
        TrayCreate(hWnd, g_hInstance);
        break;

    // ── Window destruction ────────────────────────────────────
    case WM_DESTROY:
        CloseSettingsWindow();
        TrayDestroy(hWnd);
        CleanupFeatures();
        ShutdownAudio();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// ── WinMain ───────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;
    srand((unsigned int)time(NULL));

    // COM for audio
    HRESULT hrCo = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // Single instance guard
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"KedaySingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 0;
    }

    // Load settings & init subsystems
    LoadSettings(g_settings);
    InitFeatures();

    // GDI+
    GdiplusStartupInput gsi;
    ULONG_PTR           gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gsi, NULL);

    // Register window class
    WNDCLASSW wc   = {};
    wc.style       = CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInstance;
    wc.lpszClassName = L"KedayMainWindowClass";
    wc.hCursor     = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    // Create layered, tool window (no taskbar entry, no Alt-Tab)
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    int sz = 32 * g_settings.size / 100;
    int ss = g_settings.showShadow ? 8 : 0;

    g_hMainWnd = CreateWindowExW(exStyle, L"KedayMainWindowClass", L"Keday",
                                 WS_POPUP,
                                 (int)g_nekoX, (int)g_nekoY,
                                 sz + ss, sz + ss,
                                 NULL, NULL, hInstance, NULL);
    if (!g_hMainWnd) {
        GdiplusShutdown(gdiplusToken);
        ReleaseMutex(hMutex); CloseHandle(hMutex);
        if (SUCCEEDED(hrCo)) CoUninitialize();
        return 0;
    }

    ApplyWindowSettings();
    ShowWindow(g_hMainWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hMainWnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    for (int i = 0; i < 48; ++i) {
        if (g_pSpriteFrames[i]) { delete g_pSpriteFrames[i]; g_pSpriteFrames[i] = nullptr; }
    }
    CloseSettingsWindow();
    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    if (SUCCEEDED(hrCo)) CoUninitialize();

    return (int)msg.wParam;
}
