#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <string>
#include <algorithm>
#include "settings.h"
#include "settings_window.h"
#include "resources.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;

// Constants and Macros
#define WM_TRAY_ICON (WM_USER + 1)
#define IDM_SETTINGS 2001
#define IDM_EXIT 2002
#define IDM_TOGGLE_VISIBLE 2003

// Neko States
enum NekoState {
    STATE_IDLE,
    STATE_ALERT,
    STATE_SCRATCH_SELF,
    STATE_SCRATCH_WALL_N,
    STATE_SCRATCH_WALL_S,
    STATE_SCRATCH_WALL_E,
    STATE_SCRATCH_WALL_W,
    STATE_TIRED,
    STATE_SLEEPING,
    STATE_N,
    STATE_NE,
    STATE_E,
    STATE_SE,
    STATE_S,
    STATE_SW,
    STATE_W,
    STATE_NW
};

struct Frame {
    int col;
    int row;
};

struct Animation {
    Frame frames[4];
    int frameCount;
};

// Sprites mappings (column, row in 32x32 grid)
const Animation spriteSets[] = {
    { { {3, 3} }, 1 }, // STATE_IDLE
    { { {7, 3} }, 1 }, // STATE_ALERT
    { { {5, 0}, {6, 0}, {7, 0} }, 3 }, // STATE_SCRATCH_SELF
    { { {0, 0}, {0, 1} }, 2 }, // STATE_SCRATCH_WALL_N
    { { {7, 1}, {6, 2} }, 2 }, // STATE_SCRATCH_WALL_S
    { { {2, 2}, {2, 3} }, 2 }, // STATE_SCRATCH_WALL_E
    { { {4, 0}, {4, 1} }, 2 }, // STATE_SCRATCH_WALL_W
    { { {3, 2} }, 1 }, // STATE_TIRED
    { { {2, 0}, {2, 1} }, 2 }, // STATE_SLEEPING
    { { {1, 2}, {1, 3} }, 2 }, // STATE_N
    { { {0, 2}, {0, 3} }, 2 }, // STATE_NE
    { { {3, 0}, {3, 1} }, 2 }, // STATE_E
    { { {5, 1}, {5, 2} }, 2 }, // STATE_SE
    { { {6, 3}, {7, 2} }, 2 }, // STATE_S
    { { {5, 3}, {6, 1} }, 2 }, // STATE_SW
    { { {4, 2}, {4, 3} }, 2 }, // STATE_W
    { { {1, 0}, {1, 1} }, 2 }  // STATE_NW
};

// Global Variables
HINSTANCE g_hInstance = NULL;
HWND g_hMainWnd = NULL;
Settings g_settings;
Bitmap* g_pSpriteBitmap = nullptr;
bool g_bVisible = true;

// Neko State variables
double g_nekoX = 100.0;
double g_nekoY = 100.0;
NekoState g_currentState = STATE_IDLE;
int g_currentFrameIndex = 0;
int g_idleTicks = 0;
int g_scratchTicks = 0;

// Helper: Checks if there is a fullscreen application active
bool IsFullscreenWindowActive() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls(className);
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") {
        return false;
    }
    
    RECT rcVal;
    if (GetWindowRect(hwnd, &rcVal)) {
        int w = rcVal.right - rcVal.left;
        int h = rcVal.bottom - rcVal.top;
        
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        
        if (w >= screenW && h >= screenH) {
            LONG style = GetWindowLongW(hwnd, GWL_STYLE);
            if (!(style & WS_CAPTION)) {
                return true;
            }
        }
    }
    return false;
}

// Returns the directory where the executable is located
std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos);
    }
    return dir;
}

// Loads character sprite sheet
void LoadCharacterSprite(const std::wstring& characterName) {
    if (g_pSpriteBitmap) {
        delete g_pSpriteBitmap;
        g_pSpriteBitmap = nullptr;
    }
    std::wstring baseDir = GetExeDir();
    std::wstring path = baseDir + L"\\assets\\" + characterName + L".gif";
    g_pSpriteBitmap = Bitmap::FromFile(path.c_str());
    if (!g_pSpriteBitmap || g_pSpriteBitmap->GetLastStatus() != Ok) {
        if (g_pSpriteBitmap) {
            delete g_pSpriteBitmap;
        }
        path = baseDir + L"\\assets\\" + characterName + L".png";
        g_pSpriteBitmap = Bitmap::FromFile(path.c_str());
    }
    if (!g_pSpriteBitmap || g_pSpriteBitmap->GetLastStatus() != Ok) {
        if (g_pSpriteBitmap) {
            delete g_pSpriteBitmap;
        }
        std::wstring fallback = baseDir + L"\\assets\\oneko.gif";
        g_pSpriteBitmap = Bitmap::FromFile(fallback.c_str());
    }
}

// Apply settings to window
void ApplyWindowSettings() {
    if (!g_hMainWnd) return;

    // Load sprite sheet if changed
    static std::wstring lastChar = L"";
    if (g_settings.character != lastChar) {
        LoadCharacterSprite(g_settings.character);
        lastChar = g_settings.character;
    }

    // Always on top
    HWND insertAfter = g_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g_hMainWnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

// Redraw / Update Layered Window
void UpdateNekoWindow() {
    if (!g_hMainWnd || !g_bVisible) return;

    // Check if we should hide on fullscreen
    if (g_settings.hideOnFullscreen && IsFullscreenWindowActive()) {
        ShowWindow(g_hMainWnd, SW_HIDE);
        return;
    } else {
        ShowWindow(g_hMainWnd, SW_SHOWNOACTIVATE);
    }

    int catSize = 32 * g_settings.size / 100;
    int shadowOffset = g_settings.showShadow ? 8 : 0;
    int winWidth = catSize + shadowOffset;
    int winHeight = catSize + shadowOffset;

    // Create memory DC and DIB section
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winWidth;
    bmi.bmiHeader.biHeight = -winHeight; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    // GDI+ drawing
    Graphics g(hMemDC);
    g.SetCompositingMode(CompositingModeSourceCopy);
    
    // Clear background
    SolidBrush transBrush(Color(0, 0, 0, 0));
    g.FillRectangle(&transBrush, 0, 0, winWidth, winHeight);
    
    g.SetCompositingMode(CompositingModeSourceOver);

    int spriteW = 32;
    int spriteH = 32;
    if (g_pSpriteBitmap && g_pSpriteBitmap->GetLastStatus() == Ok) {
        spriteW = g_pSpriteBitmap->GetWidth() / 8;
        spriteH = g_pSpriteBitmap->GetHeight() / 4;
    }

    // Get current animation frame
    const Animation& anim = spriteSets[g_currentState];
    Frame frame = anim.frames[g_currentFrameIndex % anim.frameCount];
    int srcX = frame.col * spriteW;
    int srcY = frame.row * spriteH;

    if (g_pSpriteBitmap && g_pSpriteBitmap->GetLastStatus() == Ok) {
        // Draw Shadow first
        if (g_settings.showShadow) {
            ImageAttributes shadowAttr;
            // 50% opacity black matrix
            ColorMatrix matrix = {
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.4f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            shadowAttr.SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
            g.DrawImage(g_pSpriteBitmap, Rect(shadowOffset, shadowOffset, catSize, catSize), srcX, srcY, spriteW, spriteH, UnitPixel, &shadowAttr);
        }

        // Draw Cat
        g.DrawImage(g_pSpriteBitmap, Rect(0, 0, catSize, catSize), srcX, srcY, spriteW, spriteH, UnitPixel);
    }

    // Update Layered Window position and size
    POINT ptDst = { (int)g_nekoX, (int)g_nekoY };
    SIZE sizeDst = { winWidth, winHeight };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = (BYTE)(g_settings.opacity * 255 / 100);
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_hMainWnd, hScreenDC, &ptDst, &sizeDst, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // Cleanup
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
}

// State Machine Update
void UpdateNekoLogic() {
    POINT mousePos;
    GetCursorPos(&mousePos);

    int catSize = 32 * g_settings.size / 100;
    
    // Neko center coordinates
    double nekoCenterX = g_nekoX + catSize / 2.0;
    double nekoCenterY = g_nekoY + catSize / 2.0;

    double dx = mousePos.x - nekoCenterX;
    double dy = mousePos.y - nekoCenterY;
    double distance = std::sqrt(dx * dx + dy * dy);

    // Speed setting map (1 to 30) -> actual movement step per frame
    double speed = std::min((double)g_settings.speed, distance * 0.5);

    bool shouldFollow = g_settings.followMouse;

    if (!shouldFollow || distance < 8.0) {
        // Idle state machine
        g_idleTicks++;
        
        if (g_currentState >= STATE_N && g_currentState <= STATE_NW) {
            // Transition from walking to idle
            g_currentState = STATE_IDLE;
            g_currentFrameIndex = 0;
        }

        if (g_idleTicks > 40) {
            // Yawn, then sleep
            if (g_currentState != STATE_SLEEPING) {
                g_currentState = STATE_SLEEPING;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 25) {
            // Tired
            if (g_currentState != STATE_TIRED) {
                g_currentState = STATE_TIRED;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 12) {
            // Random scratch or perked ears (alert)
            if (g_currentState != STATE_ALERT && g_currentState != STATE_SCRATCH_SELF) {
                if (rand() % 2 == 0) {
                    g_currentState = STATE_SCRATCH_SELF;
                } else {
                    g_currentState = STATE_ALERT;
                }
                g_currentFrameIndex = 0;
            }
        }
    } else {
        // Moving state
        g_idleTicks = 0;
        g_scratchTicks = 0;

        // Choose movement direction
        double angle = std::atan2(dy, dx) * 180.0 / 3.14159265;
        NekoState nextState = STATE_E;

        if (angle >= -22.5 && angle < 22.5) {
            nextState = STATE_E;
        } else if (angle >= 22.5 && angle < 67.5) {
            nextState = STATE_SE;
        } else if (angle >= 67.5 && angle < 112.5) {
            nextState = STATE_S;
        } else if (angle >= 112.5 && angle < 157.5) {
            nextState = STATE_SW;
        } else if (angle >= 157.5 || angle < -157.5) {
            nextState = STATE_W;
        } else if (angle >= -157.5 && angle < -112.5) {
            nextState = STATE_NW;
        } else if (angle >= -112.5 && angle < -67.5) {
            nextState = STATE_N;
        } else if (angle >= -67.5 && angle < -22.5) {
            nextState = STATE_NE;
        }

        g_currentState = nextState;

        // Move cat
        double moveX = (dx / distance) * speed;
        double moveY = (dy / distance) * speed;
        g_nekoX += moveX;
        g_nekoY += moveY;

        // Keep inside screen boundaries
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        if (g_nekoX < 0) g_nekoX = 0;
        if (g_nekoY < 0) g_nekoY = 0;
        if (g_nekoX > screenWidth - catSize) g_nekoX = screenWidth - catSize;
        if (g_nekoY > screenHeight - catSize) g_nekoY = screenHeight - catSize;

        // Edge scratching animation logic
        const int EDGE_THRESHOLD = 5;
        if (g_nekoY <= EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_N;
        } else if (g_nekoY >= screenHeight - catSize - EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_S;
        } else if (g_nekoX <= EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_W;
        } else if (g_nekoX >= screenWidth - catSize - EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_E;
        }
    }

    // Cycle frame index
    g_currentFrameIndex = (g_currentFrameIndex + 1) % spriteSets[g_currentState].frameCount;
}

// Window Procedure for transparent pet window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            // Add Tray Icon
            NOTIFYICONDATAW nid;
            memset(&nid, 0, sizeof(nid));
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = hWnd;
            nid.uID = 1;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_TRAY_ICON;
            nid.hIcon = LoadIconW(g_hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
            wcscpy_s(nid.szTip, L"Keday");
            Shell_NotifyIconW(NIM_ADD, &nid);
            break;

        case WM_TIMER:
            UpdateNekoLogic();
            UpdateNekoWindow();
            break;

        case WM_TRAY_ICON:
            if (lParam == WM_RBUTTONUP) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                
                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Ayarlar");
                AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_VISIBLE, g_bVisible ? L"Kediyi Gizle" : L"Kediyi Göster");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Çıkış");
                
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);
            } else if (lParam == WM_LBUTTONUP) {
                PostMessage(hWnd, WM_COMMAND, IDM_SETTINGS, 0);
            }
            break;

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
                    if (g_bVisible) {
                        ShowWindow(hWnd, SW_SHOWNOACTIVATE);
                        UpdateNekoWindow();
                    } else {
                        ShowWindow(hWnd, SW_HIDE);
                    }
                    break;
                case IDM_EXIT:
                    PostQuitMessage(0);
                    break;
            }
            break;

        case WM_DESTROY:
            // Remove Tray Icon
            NOTIFYICONDATAW nidDel;
            memset(&nidDel, 0, sizeof(nidDel));
            nidDel.cbSize = sizeof(NOTIFYICONDATAW);
            nidDel.hWnd = hWnd;
            nidDel.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nidDel);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Application Entry Point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Prevent multiple instances
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"KedaySingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    // Load Settings
    LoadSettings(g_settings);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register Window Class
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"KedayMainWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    // Layered window style (transparent, click-through, top-most, doesn't show in taskbar/Alt-Tab)
    DWORD dwExStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    DWORD dwStyle = WS_POPUP;

    int catSize = 32 * g_settings.size / 100;
    int shadowOffset = g_settings.showShadow ? 8 : 0;
    int winWidth = catSize + shadowOffset;
    int winHeight = catSize + shadowOffset;

    g_hMainWnd = CreateWindowExW(
        dwExStyle,
        L"KedayMainWindowClass",
        L"Keday",
        dwStyle,
        (int)g_nekoX, (int)g_nekoY,
        winWidth, winHeight,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hMainWnd) {
        GdiplusShutdown(gdiplusToken);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 0;
    }

    ApplyWindowSettings();
    ShowWindow(g_hMainWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(g_hMainWnd);

    // Tick timer (150ms)
    SetTimer(g_hMainWnd, 1, 150, NULL);

    // Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    if (g_pSpriteBitmap) {
        delete g_pSpriteBitmap;
    }

    CloseSettingsWindow();
    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (int)msg.wParam;
}
