#include <windows.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>
#include <thread>
#include <atomic>
#include <ctime>
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
#define IDM_PET 2004
#define IDM_FEED 2005

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
std::wstring g_lastLoadedChar = L"";
std::atomic<bool> g_soundPlaying{false};

// Neko State variables
double g_nekoX = 100.0;
double g_nekoY = 100.0;
NekoState g_currentState = STATE_IDLE;
int g_currentFrameIndex = 0;
int g_idleTicks = 0;
int g_scratchTicks = 0;

// Dragging & Physics state variables
bool g_isDragging = false;
POINT g_dragStartMousePos = { 0, 0 };
POINT g_dragStartWndPos = { 0, 0 };
POINT g_lastMousePos = { 0, 0 };
bool g_isFalling = false;
double g_velocityX = 0.0;
double g_velocityY = 0.0;
const double GRAVITY = 1.8;
const double BOUNCE = -0.35;

// Petting & Feeding state variables
struct HeartParticle {
    double x, y;
    double vx, vy;
    int life;
};
std::vector<HeartParticle> g_particles;

bool g_isFeeding = false;
double g_fishX = 0.0;
double g_fishY = 0.0;
int g_feedTicks = 0;

// Wandering state variables
bool g_isWanderActive = false;
double g_wanderTargetX = 0.0;
double g_wanderTargetY = 0.0;

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
    if (g_settings.character != g_lastLoadedChar) {
        LoadCharacterSprite(g_settings.character);
        g_lastLoadedChar = g_settings.character;
    }

    // Always on top
    HWND insertAfter = g_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g_hMainWnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    // Update timer based on speed
    int interval = 250 - (g_settings.speed * 6);
    if (interval < 50) interval = 50;
    SetTimer(g_hMainWnd, 1, interval, NULL);
}

// Map the animation state to coordinates on the sprite sheet depending on character choice
Frame GetCharacterFrame(NekoState state, int frameIndex, const std::wstring& character) {
    if (character == L"anime") {
        // Custom mapping for the anime chibi layout (8x8 grid)
        // Rows: 0: Down, 1: Right, 2: Left, 3: Up
        // Rows 4-7: Extra Actions/Alt sprites
        // Columns: 0, 1, 2, 3 for walking; 4, 5, 6, 7 for actions
        switch (state) {
            case STATE_IDLE:
                return { 4, 0 }; // Sitting/Idle pose down
            case STATE_ALERT:
                return { 5, 0 }; // Surprised/Alert pose
            case STATE_SCRATCH_SELF: {
                int col = 4 + (frameIndex % 4);
                return { col, 0 }; // Scratching/Wiggling poses
            }
            case STATE_SCRATCH_WALL_N: return { 5, 3 };
            case STATE_SCRATCH_WALL_S: return { 5, 0 };
            case STATE_SCRATCH_WALL_E: return { 5, 1 };
            case STATE_SCRATCH_WALL_W: return { 5, 2 };
            case STATE_TIRED:
                return { 4, 1 }; // Sitting down side
            case STATE_SLEEPING: {
                int col = 4 + (frameIndex % 4);
                return { col, 1 }; // Sleep/lying poses on row 1
            }
            // Walking Up
            case STATE_N: {
                int col = frameIndex % 4;
                return { col, 3 };
            }
            // Diagonal walking Up-East/West
            case STATE_NE: {
                int col = frameIndex % 4;
                return { col, 1 }; // Walk Right (Row 1)
            }
            case STATE_NW: {
                int col = frameIndex % 4;
                return { col, 2 }; // Walk Left (Row 2)
            }
            // Walking Down
            case STATE_S: {
                int col = frameIndex % 4;
                return { col, 0 };
            }
            // Diagonal walking Down-East/West
            case STATE_SE: {
                int col = frameIndex % 4;
                return { col, 1 }; // Walk Right (Row 1)
            }
            case STATE_SW: {
                int col = frameIndex % 4;
                return { col, 2 }; // Walk Left (Row 2)
            }
            // Walking East (Right)
            case STATE_E: {
                int col = frameIndex % 4;
                return { col, 1 }; // Row 1 is Right-walking
            }
            // Walking West (Left)
            case STATE_W: {
                int col = frameIndex % 4;
                return { col, 2 }; // Row 2 is Left-walking
            }
            default:
                return { 0, 0 };
        }
    } else {
        const Animation& anim = spriteSets[state];
        return anim.frames[frameIndex % anim.frameCount];
    }
}

int GetCharacterFrameCount(NekoState state, const std::wstring& character) {
    if (character == L"anime") {
        switch (state) {
            case STATE_SCRATCH_SELF: return 4;
            case STATE_SLEEPING: return 4;
            case STATE_IDLE:
            case STATE_ALERT:
            case STATE_TIRED:
            case STATE_SCRATCH_WALL_N:
            case STATE_SCRATCH_WALL_S:
            case STATE_SCRATCH_WALL_E:
            case STATE_SCRATCH_WALL_W:
                return 1;
            default:
                return 4; // 4 walking animation frames
        }
    } else {
        return spriteSets[state].frameCount;
    }
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
    
    // Add extra padding around window to draw hearts and falling fish above/around the cat
    int extraTop = 40;
    int extraSide = 10;
    int winWidth = catSize + shadowOffset + (extraSide * 2);
    int winHeight = catSize + shadowOffset + extraTop;

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
        if (g_settings.character == L"anime") {
            spriteH = g_pSpriteBitmap->GetHeight() / 8;
        } else {
            spriteH = g_pSpriteBitmap->GetHeight() / 4;
        }
    }

    // Get current animation frame
    Frame frame = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
    int srcX = frame.col * spriteW;
    int srcY = frame.row * spriteH;

    if (g_pSpriteBitmap && g_pSpriteBitmap->GetLastStatus() == Ok) {
        // Draw Shadow first
        if (g_settings.showShadow) {
            ImageAttributes shadowAttr;
            ColorMatrix matrix = {
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.4f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f, 1.0f
            };
            shadowAttr.SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
            g.DrawImage(g_pSpriteBitmap, Rect(extraSide + shadowOffset, extraTop + shadowOffset, catSize, catSize), srcX, srcY, spriteW, spriteH, UnitPixel, &shadowAttr);
        }

        // Draw Cat
        g.DrawImage(g_pSpriteBitmap, Rect(extraSide, extraTop, catSize, catSize), srcX, srcY, spriteW, spriteH, UnitPixel);
    }

    // Draw Falling Fish
    if (g_isFeeding) {
        SolidBrush fishBrush(Color(255, 60, 150, 240));
        Font fishFont(L"Segoe UI Emoji", 14, FontStyleRegular);
        g.DrawString(L"🐟", -1, &fishFont, PointF((float)(g_fishX + extraSide), (float)g_fishY), &fishBrush);
    }

    // Draw Particles
    if (!g_particles.empty()) {
        SolidBrush heartBrush(Color(255, 255, 80, 120));
        Font heartFont(L"Segoe UI Emoji", 10, FontStyleRegular);
        for (const auto& p : g_particles) {
            g.DrawString(L"♥", -1, &heartFont, PointF((float)(p.x + extraSide), (float)p.y), &heartBrush);
        }
    }

    // Update Layered Window position and size
    POINT ptDst = { (int)g_nekoX - extraSide, (int)g_nekoY - extraTop };
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

// Check if Keday is standing on the active window ledge
bool IsStandingOnWindowLedge(int x, int y, int catSize, int& outLedgeY) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == g_hMainWnd || IsIconic(hwnd)) return false;

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls(className);
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") {
        return false;
    }

    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        int catCenterX = x + catSize / 2;
        if (catCenterX >= rc.left && catCenterX <= rc.right) {
            int catBottom = y + catSize;
            if (std::abs(catBottom - rc.top) <= 3) {
                outLedgeY = rc.top;
                return true;
            }
        }
    }
    return false;
}

// Check if falling Keday spanned across active window ledge
bool IsSpanningWindowLedge(int x, int y, double velocityY, int catSize, int& outLedgeY) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == g_hMainWnd || IsIconic(hwnd)) return false;

    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls(className);
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") {
        return false;
    }

    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        int catCenterX = x + catSize / 2;
        if (catCenterX >= rc.left && catCenterX <= rc.right) {
            double prevBottom = y + catSize - velocityY;
            double currBottom = y + catSize;
            if (prevBottom <= rc.top + 2 && currBottom >= rc.top - 8) {
                outLedgeY = rc.top;
                return true;
            }
        }
    }
    return false;
}

// Play an 8-bit meow sound asynchronously on a background thread
void PlayMeowAsync() {
    if (g_soundPlaying.exchange(true)) return;
    std::thread([]() {
        Beep(650, 45);
        Beep(850, 50);
        Beep(1100, 110);
        g_soundPlaying = false;
    }).detach();
}

// Play an 8-bit purr sound asynchronously on a background thread
void PlayPurrAsync() {
    if (g_soundPlaying.exchange(true)) return;
    std::thread([]() {
        for (int i = 0; i < 3; ++i) {
            Beep(160, 35);
            Sleep(90);
        }
        g_soundPlaying = false;
    }).detach();
}

// Spawn heart particles above Keday
void SpawnHearts(double startX, double startY, int count) {
    for (int i = 0; i < count; ++i) {
        HeartParticle p;
        p.x = startX + (rand() % 24 - 12);
        p.y = startY + (rand() % 10 - 5);
        p.vx = (rand() % 100 - 50) / 100.0;
        p.vy = -(rand() % 100 + 50) / 50.0;
        p.life = 12 + rand() % 8;
        g_particles.push_back(p);
    }
}

// State Machine Update
void UpdateNekoLogic() {
    POINT mousePos;
    GetCursorPos(&mousePos);

    int catSize = 32 * g_settings.size / 100;

    // Update floating heart particles
    for (auto it = g_particles.begin(); it != g_particles.end(); ) {
        it->x += it->vx;
        it->y += it->vy;
        it->life--;
        if (it->life <= 0) {
            it = g_particles.erase(it);
        } else {
            ++it;
        }
    }

    // Lock cat animation when eating/licking paws after food
    if (g_feedTicks > 0) {
        g_feedTicks--;
        g_currentState = STATE_SCRATCH_SELF;
        g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);
        return;
    }

    // Handle falling food
    if (g_isFeeding) {
        g_fishY += 6.0; // make the fish fall down
        if (g_fishY >= 35.0) {
            PlayMeowAsync();
            g_isFeeding = false;
            g_feedTicks = 16; // lick paws for 16 ticks
            g_currentState = STATE_SCRATCH_SELF;
            g_currentFrameIndex = 0;
            // Spawn happy hearts at the top of the cat's head
            SpawnHearts(catSize / 2.0, 30.0, 12);
        } else {
            g_currentState = STATE_ALERT; // look up expectantly
            g_currentFrameIndex = 0;
        }
        return;
    }

    if (g_isDragging) {
        // Track dragging velocity
        g_velocityX = mousePos.x - g_lastMousePos.x;
        g_velocityY = mousePos.y - g_lastMousePos.y;
        
        // Clamp to prevent excessive speed on release
        if (g_velocityX > 35.0) g_velocityX = 35.0;
        if (g_velocityX < -35.0) g_velocityX = -35.0;
        if (g_velocityY > 35.0) g_velocityY = 35.0;
        if (g_velocityY < -35.0) g_velocityY = -35.0;

        g_lastMousePos = mousePos;

        // Visual while dragging
        g_currentState = STATE_ALERT;
        g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);
        return;
    }

    if (g_isFalling) {
        g_velocityY += GRAVITY;
        const double MAX_VELOCITY_Y = 24.0;
        if (g_velocityY > MAX_VELOCITY_Y) g_velocityY = MAX_VELOCITY_Y;
        g_nekoX += g_velocityX;
        g_nekoY += g_velocityY;

        g_velocityX *= 0.98; // Air resistance

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        // Ceiling collision
        if (g_nekoY < 0) {
            g_nekoY = 0;
            g_velocityY = 0;
        }

        // Wall collisions
        if (g_nekoX < 0) {
            g_nekoX = 0;
            g_velocityX = -g_velocityX * 0.5;
        }
        if (g_nekoX > screenWidth - catSize) {
            g_nekoX = screenWidth - catSize;
            g_velocityX = -g_velocityX * 0.5;
        }

        // Active window ledge collision check
        int ledgeY = 0;
        if (g_velocityY > 0 && IsSpanningWindowLedge((int)g_nekoX, (int)g_nekoY, g_velocityY, catSize, ledgeY)) {
            g_nekoY = ledgeY - catSize;
            if (std::abs(g_velocityY) > 6.0) {
                g_velocityY = g_velocityY * BOUNCE;
            } else {
                g_isFalling = false;
                g_velocityY = 0;
                g_velocityX = 0;
                g_currentState = STATE_TIRED;
                g_idleTicks = 0;
                g_currentFrameIndex = 0;
            }
        }

        // Floor collision
        if (g_nekoY >= screenHeight - catSize) {
            g_nekoY = screenHeight - catSize;
            if (std::abs(g_velocityY) > 6.0) {
                g_velocityY = g_velocityY * BOUNCE;
            } else {
                g_isFalling = false;
                g_velocityY = 0;
                g_velocityX = 0;
                g_currentState = STATE_TIRED;
                g_idleTicks = 0;
                g_currentFrameIndex = 0;
            }
        }

        if (g_isFalling) {
            g_currentState = STATE_ALERT; // scared state while falling
        }
        g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);
        
        // Update window position during fall
        SetWindowPos(g_hMainWnd, NULL, (int)g_nekoX, (int)g_nekoY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    // If not dragging/falling and we are in mid-air, start falling (only if not following mouse)
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int tempLedgeY = 0;
    bool onLedge = IsStandingOnWindowLedge((int)g_nekoX, (int)g_nekoY, catSize, tempLedgeY);
    if (!g_settings.followMouse && g_nekoY < screenHeight - catSize && !onLedge) {
        g_isFalling = true;
        g_isWanderActive = false;
    }
    
    // Neko center coordinates
    double nekoCenterX = g_nekoX + catSize / 2.0;
    double nekoCenterY = g_nekoY + catSize / 2.0;

    bool shouldFollow = g_settings.followMouse;

    // Handle random wandering if not following mouse
    if (!shouldFollow) {
        if (!g_isWanderActive) {
            if (g_idleTicks > 100 && rand() % 40 == 0) {
                g_isWanderActive = true;
                int ledgeY = 0;
                if (IsStandingOnWindowLedge((int)g_nekoX, (int)g_nekoY, catSize, ledgeY)) {
                    g_wanderTargetY = ledgeY - catSize;
                    HWND hwnd = GetForegroundWindow();
                    RECT rc;
                    if (GetWindowRect(hwnd, &rc)) {
                        int minX = rc.left;
                        int maxX = rc.right - catSize;
                        if (maxX > minX) {
                            g_wanderTargetX = minX + (rand() % (maxX - minX));
                        } else {
                            g_wanderTargetX = minX;
                        }
                    } else {
                        g_wanderTargetX = rand() % (GetSystemMetrics(SM_CXSCREEN) - catSize);
                    }
                } else {
                    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
                    int deltaX = rand() % 400 - 200;
                    g_wanderTargetX = g_nekoX + deltaX;
                    if (g_wanderTargetX < 0) g_wanderTargetX = 0;
                    if (g_wanderTargetX > screenWidth - catSize) g_wanderTargetX = screenWidth - catSize;
                    g_wanderTargetY = GetSystemMetrics(SM_CYSCREEN) - catSize;
                }
            }
        }
        
        if (g_isWanderActive) {
            shouldFollow = true;
        }
    }

    double targetX = shouldFollow ? (g_isWanderActive ? g_wanderTargetX : mousePos.x) : nekoCenterX;
    double targetY = shouldFollow ? (g_isWanderActive ? g_wanderTargetY : mousePos.y) : nekoCenterY;

    double dx = targetX - nekoCenterX;
    double dy = targetY - nekoCenterY;
    double distance = std::sqrt(dx * dx + dy * dy);

    // Speed setting map (1 to 30) -> actual movement step per frame
    double speed = std::min((double)g_settings.speed, distance * 0.5);

    if (g_isWanderActive && distance < 8.0) {
        g_isWanderActive = false;
        g_idleTicks = 0;
        shouldFollow = false;
    }

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
    g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);
}

// Window Procedure for transparent pet window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_NCHITTEST: {
            POINT ptMouse;
            ptMouse.x = GET_X_LPARAM(lParam);
            ptMouse.y = GET_Y_LPARAM(lParam);
            
            POINT ptClient = ptMouse;
            ScreenToClient(hWnd, &ptClient);
            
            int catSize = 32 * g_settings.size / 100;
            int extraTop = 40;
            int extraSide = 10;
            
            int catLeft = extraSide;
            int catRight = extraSide + catSize;
            int catTop = extraTop;
            int catBottom = extraTop + catSize;
            
            if (ptClient.x >= catLeft && ptClient.x < catRight && ptClient.y >= catTop && ptClient.y < catBottom) {
                if (g_pSpriteBitmap && g_pSpriteBitmap->GetLastStatus() == Ok) {
                    Frame frame = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
                    int spriteW = g_pSpriteBitmap->GetWidth() / 8;
                    int spriteH = (g_settings.character == L"anime") ? (g_pSpriteBitmap->GetHeight() / 8) : (g_pSpriteBitmap->GetHeight() / 4);
                    
                    int relativeX = ptClient.x - catLeft;
                    int relativeY = ptClient.y - catTop;
                    
                    int pixelX = frame.col * spriteW + (relativeX * spriteW / catSize);
                    int pixelY = frame.row * spriteH + (relativeY * spriteH / catSize);
                    
                    if (pixelX >= 0 && pixelX < (int)g_pSpriteBitmap->GetWidth() &&
                        pixelY >= 0 && pixelY < (int)g_pSpriteBitmap->GetHeight()) {
                        Color c;
                        g_pSpriteBitmap->GetPixel(pixelX, pixelY, &c);
                        if (c.GetAlpha() > 20) {
                            return HTCLIENT;
                        }
                    }
                }
            }
            return HTTRANSPARENT;
        }

        case WM_LBUTTONDOWN: {
            PlayMeowAsync();
            g_isDragging = true;
            g_isFalling = false;
            g_velocityX = 0.0;
            g_velocityY = 0.0;
            SetCapture(hWnd);
            
            GetCursorPos(&g_dragStartMousePos);
            g_lastMousePos = g_dragStartMousePos;
            
            RECT rc;
            GetWindowRect(hWnd, &rc);
            g_dragStartWndPos.x = rc.left;
            g_dragStartWndPos.y = rc.top;
            
            g_currentState = STATE_ALERT;
            g_currentFrameIndex = 0;
            UpdateNekoWindow();
            break;
        }

        case WM_MOUSEMOVE: {
            if (g_isDragging) {
                POINT curMouse;
                GetCursorPos(&curMouse);
                int dx = curMouse.x - g_dragStartMousePos.x;
                int dy = curMouse.y - g_dragStartMousePos.y;
                
                g_nekoX = g_dragStartWndPos.x + dx;
                g_nekoY = g_dragStartWndPos.y + dy;
                
                SetWindowPos(hWnd, NULL, (int)g_nekoX, (int)g_nekoY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                UpdateNekoWindow();
            }
            break;
        }

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

        case WM_RBUTTONUP: {
            POINT curPoint;
            GetCursorPos(&curPoint);
            
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, IDM_PET, L"Sev");
            AppendMenuW(hMenu, MF_STRING, IDM_FEED, L"Yem Ver");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Ayarlar");
            AppendMenuW(hMenu, MF_STRING, IDM_TOGGLE_VISIBLE, g_bVisible ? L"Kediyi Gizle" : L"Kediyi Göster");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Çıkış");
            
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hWnd, NULL);
            DestroyMenu(hMenu);
            break;
        }

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
                AppendMenuW(hMenu, MF_STRING, IDM_PET, L"Sev");
                AppendMenuW(hMenu, MF_STRING, IDM_FEED, L"Yem Ver");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
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
                case IDM_PET: {
                    PlayPurrAsync();
                    int catSize = 32 * g_settings.size / 100;
                    SpawnHearts(catSize / 2.0, 25.0, 8);
                    g_feedTicks = 12;
                    g_currentState = STATE_SCRATCH_SELF;
                    g_currentFrameIndex = 0;
                    UpdateNekoWindow();
                    break;
                }
                case IDM_FEED: {
                    if (!g_isFeeding) {
                        g_isFeeding = true;
                        int catSize = 32 * g_settings.size / 100;
                        g_fishX = catSize / 2.0;
                        g_fishY = -20.0;
                        g_currentState = STATE_ALERT;
                        g_currentFrameIndex = 0;
                        UpdateNekoWindow();
                    }
                    break;
                }
                case IDM_EXIT:
                    CloseSettingsWindow();
                    DestroyWindow(hWnd);
                    break;
            }
            break;

        case WM_DESTROY:
            CloseSettingsWindow();
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
    srand((unsigned int)time(NULL));

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

    // Layered window style (transparent, input-aware, top-most, doesn't show in taskbar/Alt-Tab)
    DWORD dwExStyle = WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
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
