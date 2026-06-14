#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

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
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include "settings.h"
#include "settings_window.h"
#include "resources.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;

// Forward Declarations
void DrawAccessory(Graphics& g, int x, int y, int size, int type);
const double M_PI = 3.14159265358979323846;

// Define IAudioMeterInformation if not defined or incomplete in MinGW headers
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
DEFINE_GUID(IID_IAudioMeterInformation, 0xC02216C6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64);
interface DECLSPEC_UUID("C02216C6-8C67-4B5B-9D00-D008E73E0064") IAudioMeterInformation : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float *pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT *pnChannelCount) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 u32ChannelCount, float *afPeakValues) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD *pdwHardwareSupportMask) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IAudioMeterInformation, 0xC02216C6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64)
#endif
#endif



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
    STATE_NW,
    STATE_EXPECT_FOOD,
    STATE_EATING,
    STATE_CODING,
    STATE_BOX
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
    { { {1, 0}, {1, 1} }, 2 }, // STATE_NW
    { { {0, 4} }, 1 }, // STATE_EXPECT_FOOD
    { { {1, 4}, {2, 4}, {3, 4}, {4, 4} }, 4 }, // STATE_EATING
    { { {0, 5}, {1, 5}, {2, 5}, {3, 5} }, 4 }, // STATE_CODING
    { { {4, 5}, {5, 5}, {6, 5}, {7, 5} }, 4 }  // STATE_BOX
};

// Global Variables
HINSTANCE g_hInstance = NULL;
HWND g_hMainWnd = NULL;
Settings g_settings;
Bitmap* g_pSpriteFrames[48] = { nullptr };
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
    wchar_t symbol = L'♥';
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

// Loads character sprites from individual folder frames
void LoadCharacterSprite(const std::wstring& characterName) {
    int k = 0;
    // Clean up old frames
    for (k = 0; k < 48; ++k) {
        if (g_pSpriteFrames[k]) {
            delete g_pSpriteFrames[k];
            g_pSpriteFrames[k] = nullptr;
        }
    }
    
    std::wstring baseDir = GetExeDir();
    // Path to the character's folder: assets/<characterName>
    for (k = 0; k < 48; ++k) {
        std::wstring path = baseDir + L"\\assets\\" + characterName + L"\\" + std::to_wstring(k) + L".png";
        g_pSpriteFrames[k] = Bitmap::FromFile(path.c_str());
        
        // Fallback: If not found, try falling back to oneko directory
        if (!g_pSpriteFrames[k] || g_pSpriteFrames[k]->GetLastStatus() != Ok) {
            if (g_pSpriteFrames[k]) {
                delete g_pSpriteFrames[k];
            }
            std::wstring fallbackPath = baseDir + L"\\assets\\oneko\\" + std::to_wstring(k) + L".png";
            g_pSpriteFrames[k] = Bitmap::FromFile(fallbackPath.c_str());
        }
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

    // Click through initial style
    LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
    if (g_settings.clickThrough) {
        if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
            exStyle |= WS_EX_TRANSPARENT;
        } else {
            exStyle &= ~WS_EX_TRANSPARENT;
        }
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(g_hMainWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // Update timer based on speed
    int interval = 250 - (g_settings.speed * 6);
    if (interval < 50) interval = 50;
    SetTimer(g_hMainWnd, 1, interval, NULL);
}

// Map the animation state to coordinates on the sprite sheet depending on character choice
Frame GetCharacterFrame(NekoState state, int frameIndex, const std::wstring& character) {
    const Animation& anim = spriteSets[state];
    return anim.frames[frameIndex % anim.frameCount];
}

int GetCharacterFrameCount(NekoState state, const std::wstring& character) {
    return spriteSets[state].frameCount;
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

    // Get current animation frame
    Frame frame = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
    int frameIdx = frame.row * 8 + frame.col;

    if (frameIdx >= 0 && frameIdx < 48 && g_pSpriteFrames[frameIdx] && g_pSpriteFrames[frameIdx]->GetLastStatus() == Ok) {
        Bitmap* pBmp = g_pSpriteFrames[frameIdx];
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
            g.DrawImage(pBmp, Rect(extraSide + shadowOffset, extraTop + shadowOffset, catSize, catSize), 0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel, &shadowAttr);
        }

        // Draw Cat (supporting toss-and-spin rotation)
        if (g_isFalling && (std::abs(g_velocityX) > 2.0 || std::abs(g_velocityY) > 2.0)) {
            static float angle = 0.0f;
            angle += (float)(g_velocityX * 1.5);
            g.TranslateTransform(extraSide + catSize / 2.0f, extraTop + catSize / 2.0f);
            g.RotateTransform(angle);
            g.DrawImage(pBmp, Rect(-catSize / 2, -catSize / 2, catSize, catSize), 0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel);
            g.ResetTransform();
            
            // Draw accessory rotated with the cat
            g.TranslateTransform(extraSide + catSize / 2.0f, extraTop + catSize / 2.0f);
            g.RotateTransform(angle);
            DrawAccessory(g, -catSize / 2, -catSize / 2, catSize, g_settings.accessory);
            g.ResetTransform();
        } else {
            g.DrawImage(pBmp, Rect(extraSide, extraTop, catSize, catSize), 0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel);
            DrawAccessory(g, extraSide, extraTop, catSize, g_settings.accessory);
        }
    }

    // Draw Falling Fish
    if (g_isFeeding) {
        SolidBrush fishBrush(Color(255, 60, 150, 240));
        Font fishFont(L"Segoe UI Emoji", 14, FontStyleRegular);
        g.DrawString(L"🐟", -1, &fishFont, PointF((float)(g_fishX + extraSide), (float)g_fishY), &fishBrush);
    }

    // Draw Particles (Hearts and Zzz)
    if (!g_particles.empty()) {
        Font particleFont(L"Segoe UI Emoji", 9, FontStyleBold);
        for (const auto& p : g_particles) {
            if (p.symbol == 'Z') {
                SolidBrush zBrush(Color(255, 130, 180, 255));
                Font zFont(L"Segoe UI", (REAL)(6 + (p.life % 4)), FontStyleBold);
                wchar_t str[2] = { p.symbol, L'\0' };
                g.DrawString(str, -1, &zFont, PointF((float)(p.x + extraSide), (float)p.y), &zBrush);
            } else {
                SolidBrush heartBrush(Color(255, 255, 80, 120));
                wchar_t str[2] = { p.symbol, L'\0' };
                g.DrawString(str, -1, &particleFont, PointF((float)(p.x + extraSide), (float)p.y), &heartBrush);
            }
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

#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>

int g_happinessLevel = 80;

// Play a dynamically generated PCM sine wave sound in memory with precise volume control
void PlaySoundWave(double frequency, int durationMs, int volumePercent) {
    if (volumePercent <= 0) return;
    int sampleRate = 22050;
    int numSamples = (sampleRate * durationMs) / 1000;
    
    struct WAVHeader {
        char chunkID[4] = {'R', 'I', 'F', 'F'};
        int chunkSize;
        char format[4] = {'W', 'A', 'V', 'E'};
        char subchunk1ID[4] = {'f', 'm', 't', ' '};
        int subchunk1Size = 16;
        short audioFormat = 1; // PCM
        short numChannels = 1; // Mono
        int sampleRate;
        int byteRate;
        short blockAlign = 1;
        short bitsPerSample = 8;
        char subchunk2ID[4] = {'d', 'a', 't', 'a'};
        int subchunk2Size;
    } header;
    
    header.sampleRate = sampleRate;
    header.byteRate = sampleRate;
    header.subchunk2Size = numSamples;
    header.chunkSize = 36 + numSamples;
    
    std::vector<char> wavData(sizeof(WAVHeader) + numSamples);
    double amplitude = (volumePercent / 100.0) * 127.0;
    char* pData = wavData.data() + sizeof(WAVHeader);
    
    for (int i = 0; i < numSamples; ++i) {
        double t = (double)i / sampleRate;
        double angle = 2.0 * M_PI * frequency * t;
        pData[i] = (char)(128 + amplitude * sin(angle));
    }
    
    memcpy(wavData.data(), &header, sizeof(WAVHeader));
    
    std::thread([data = std::move(wavData)]() {
        PlaySoundW((LPCWSTR)data.data(), NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
    }).detach();
}

// Play character specific sounds asynchronously on a background thread
void PlayCharacterSoundAsync(const std::wstring& character, int volume) {
    if (g_soundPlaying.exchange(true)) return;
    std::thread([character, volume]() {
        if (character == L"dog") {
            PlaySoundWave(200.0, 70, volume);
            Sleep(80);
            PlaySoundWave(230.0, 90, volume);
        } else if (character == L"sakura" || character == L"tomoyo") {
            PlaySoundWave(980.0, 50, volume);
            Sleep(60);
            PlaySoundWave(1300.0, 80, volume);
        } else if (character == L"bsd") {
            PlaySoundWave(400.0, 40, volume);
            Sleep(50);
            PlaySoundWave(600.0, 40, volume);
            Sleep(50);
            PlaySoundWave(800.0, 60, volume);
        } else {
            PlaySoundWave(650.0, 50, volume);
            Sleep(60);
            PlaySoundWave(850.0, 60, volume);
            Sleep(70);
            PlaySoundWave(1100.0, 120, volume);
        }
        g_soundPlaying = false;
    }).detach();
}

// Play an 8-bit meow sound asynchronously on a background thread
void PlayMeowAsync() {
    PlayCharacterSoundAsync(g_settings.character, g_settings.volume);
}

// Play an 8-bit purr sound asynchronously on a background thread
void PlayPurrAsync() {
    if (g_soundPlaying.exchange(true)) return;
    int vol = g_settings.volume;
    std::thread([vol]() {
        for (int i = 0; i < 3; ++i) {
            PlaySoundWave(160.0, 35, vol);
            Sleep(90);
        }
        g_soundPlaying = false;
    }).detach();
}

// Draw scaled procedurally rendered accessories on top of Keday
void DrawAccessory(Graphics& g, int x, int y, int size, int type) {
    if (type <= 0) return;
    int offsetHeadY = 0;
    if (g_currentState == STATE_SLEEPING) {
        offsetHeadY = size / 6;
    }
    if (type == 1) {
        // Glasses
        Pen glassesPen(Color(255, 0, 0, 0), size / 20.0f);
        g.DrawRectangle(&glassesPen, x + (int)(size * 0.32), y + (int)(size * 0.38) + offsetHeadY, (int)(size * 0.16), (int)(size * 0.12));
        g.DrawRectangle(&glassesPen, x + (int)(size * 0.52), y + (int)(size * 0.38) + offsetHeadY, (int)(size * 0.16), (int)(size * 0.12));
        g.DrawLine(&glassesPen, x + (int)(size * 0.48), y + (int)(size * 0.44) + offsetHeadY, x + (int)(size * 0.52), y + (int)(size * 0.44) + offsetHeadY);
    } 
    else if (type == 2) {
        // Santa Hat
        SolidBrush redBrush(Color(255, 230, 40, 40));
        SolidBrush whiteBrush(Color(255, 245, 245, 245));
        Point points[] = {
            Point(x + (int)(size * 0.35), y + (int)(size * 0.22) + offsetHeadY),
            Point(x + (int)(size * 0.65), y + (int)(size * 0.22) + offsetHeadY),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.04) + offsetHeadY)
        };
        g.FillPolygon(&redBrush, points, 3);
        g.FillRectangle(&whiteBrush, x + (int)(size * 0.30), y + (int)(size * 0.20) + offsetHeadY, (int)(size * 0.40), (int)(size * 0.06));
        g.FillEllipse(&whiteBrush, x + (int)(size * 0.46), y + (int)(size * 0.01) + offsetHeadY, (int)(size * 0.08), (int)(size * 0.08));
    } 
    else if (type == 3) {
        // Bow Tie
        SolidBrush redBrush(Color(255, 220, 20, 60));
        SolidBrush centerBrush(Color(255, 150, 10, 40));
        Point leftWing[] = {
            Point(x + (int)(size * 0.38), y + (int)(size * 0.56) + offsetHeadY),
            Point(x + (int)(size * 0.38), y + (int)(size * 0.68) + offsetHeadY),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.62) + offsetHeadY)
        };
        g.FillPolygon(&redBrush, leftWing, 3);
        Point rightWing[] = {
            Point(x + (int)(size * 0.62), y + (int)(size * 0.56) + offsetHeadY),
            Point(x + (int)(size * 0.62), y + (int)(size * 0.68) + offsetHeadY),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.62) + offsetHeadY)
        };
        g.FillPolygon(&redBrush, rightWing, 3);
        g.FillEllipse(&centerBrush, x + (int)(size * 0.47), y + (int)(size * 0.59) + offsetHeadY, (int)(size * 0.06), (int)(size * 0.06));
    }
}

// Spawn heart particles above Keday
void SpawnHearts(double startX, double startY, int count, wchar_t symbol = L'♥') {
    for (int i = 0; i < count; ++i) {
        HeartParticle p;
        p.x = startX + (rand() % 24 - 12);
        p.y = startY + (rand() % 10 - 5);
        p.vx = (rand() % 100 - 50) / 100.0;
        p.vy = -(rand() % 100 + 50) / 50.0;
        p.life = 12 + rand() % 8;
        p.symbol = symbol;
        g_particles.push_back(p);
    }
}

void GetNekoMonitorBounds(int x, int y, RECT& rect) {
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMon, &mi)) {
        rect = mi.rcWork;
    } else {
        rect.left = 0;
        rect.top = 0;
        rect.right = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
}

float GetSystemAudioPeakVolume() {
    float peak = 0.0f;
    IMMDeviceEnumerator* pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (SUCCEEDED(hr)) {
        IMMDevice* pDevice = NULL;
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (SUCCEEDED(hr)) {
            IAudioMeterInformation* pMeter = NULL;
            hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeter);
            if (SUCCEEDED(hr)) {
                pMeter->GetPeakValue(&peak);
                pMeter->Release();
            }
            pDevice->Release();
        }
        pEnumerator->Release();
    }
    return peak;
}

// State Machine Update
bool UpdateNekoLogic() {
    // Dynamic click-through handling based on Ctrl key
    if (g_hMainWnd) {
        LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
        LONG_PTR newStyle = exStyle;
        if (g_settings.clickThrough) {
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                newStyle &= ~WS_EX_TRANSPARENT;
            } else {
                newStyle |= WS_EX_TRANSPARENT;
            }
        } else {
            newStyle &= ~WS_EX_TRANSPARENT;
        }
        if (newStyle != exStyle) {
            SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, newStyle);
            SetWindowPos(g_hMainWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
    }

    double oldX = g_nekoX;
    double oldY = g_nekoY;
    NekoState oldState = g_currentState;
    int oldFrameIndex = g_currentFrameIndex;
    size_t oldParticleCount = g_particles.size();

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
        if (g_currentState != STATE_EATING) {
            g_currentState = STATE_SCRATCH_SELF;
        }
        g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);
        return true;
    }

    // Handle falling food
    if (g_isFeeding) {
        g_fishY += 6.0; // make the fish fall down
        if (g_fishY >= 35.0) {
            PlayMeowAsync();
            g_isFeeding = false;
            g_feedTicks = 20; // 20 ticks of eating animation (5 loops of 4 frames)
            g_currentState = STATE_EATING;
            g_currentFrameIndex = 0;
            // Spawn happy hearts at the top of the cat's head
            SpawnHearts(catSize / 2.0, 30.0, 12);
        } else {
            g_currentState = STATE_EXPECT_FOOD; // look up expectantly (frame 32)
            g_currentFrameIndex = 0;
        }
        return true;
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
        return true;
    }

    if (g_isFalling) {
        g_velocityY += GRAVITY;
        const double MAX_VELOCITY_Y = 24.0;
        if (g_velocityY > MAX_VELOCITY_Y) g_velocityY = MAX_VELOCITY_Y;
        g_nekoX += g_velocityX;
        g_nekoY += g_velocityY;

        g_velocityX *= 0.98; // Air resistance

        RECT rWork;
        GetNekoMonitorBounds((int)g_nekoX, (int)g_nekoY, rWork);

        int screenLeft = rWork.left;
        int screenTop = rWork.top;
        int screenWidth = rWork.right;
        int screenHeight = rWork.bottom;

        // Ceiling collision
        if (g_nekoY < screenTop) {
            g_nekoY = screenTop;
            g_velocityY = 0;
        }

        // Wall collisions
        if (g_nekoX < screenLeft) {
            g_nekoX = screenLeft;
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
        return true;
    }

    // If not dragging/falling and we are in mid-air, start falling (only if not following mouse)
    RECT rWork;
    GetNekoMonitorBounds((int)g_nekoX, (int)g_nekoY, rWork);
    int screenHeight = rWork.bottom;
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

        if (g_idleTicks > 120) {
            // AFK: Sleep, Code or Box
            if (g_currentState != STATE_SLEEPING && g_currentState != STATE_CODING && g_currentState != STATE_BOX) {
                int r = rand() % 3;
                if (r == 0) {
                    g_currentState = STATE_SLEEPING;
                } else if (r == 1) {
                    g_currentState = STATE_CODING;
                } else {
                    g_currentState = STATE_BOX;
                }
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 40) {
            // Yawn, then sleep
            if (g_currentState != STATE_SLEEPING && g_currentState != STATE_CODING && g_currentState != STATE_BOX) {
                g_currentState = STATE_SLEEPING;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 25) {
            // Tired
            if (g_currentState != STATE_TIRED && g_currentState != STATE_CODING && g_currentState != STATE_BOX) {
                g_currentState = STATE_TIRED;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 12) {
            // Random scratch or perked ears (alert)
            if (g_currentState != STATE_ALERT && g_currentState != STATE_SCRATCH_SELF && g_currentState != STATE_CODING && g_currentState != STATE_BOX) {
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

        // Keep inside screen boundaries on current monitor
        RECT rWork;
        GetNekoMonitorBounds((int)g_nekoX, (int)g_nekoY, rWork);
        int screenLeft = rWork.left;
        int screenTop = rWork.top;
        int screenWidth = rWork.right;
        int screenHeight = rWork.bottom;

        if (g_nekoX < screenLeft) g_nekoX = screenLeft;
        if (g_nekoY < screenTop) g_nekoY = screenTop;
        if (g_nekoX > screenWidth - catSize) g_nekoX = screenWidth - catSize;
        if (g_nekoY > screenHeight - catSize) g_nekoY = screenHeight - catSize;

        // Edge scratching animation logic
        const int EDGE_THRESHOLD = 5;
        if (g_nekoY <= screenTop + EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_N;
        } else if (g_nekoY >= screenHeight - catSize - EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_S;
        } else if (g_nekoX <= screenLeft + EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_W;
        } else if (g_nekoX >= screenWidth - catSize - EDGE_THRESHOLD) {
            g_currentState = STATE_SCRATCH_WALL_E;
        }
    }

    // 1. Zzz Sleep Particles
    if (g_currentState == STATE_SLEEPING) {
        if (rand() % 12 == 0) {
            SpawnHearts(catSize / 2.0, 20.0, 1, L'Z');
        }
    }

    // 2. Keyboard Typing Activity Detector
    static int s_keyPressCount = 0;
    static int s_typingTimer = 0;
    s_typingTimer++;
    bool keyPressed = false;
    for (int k = 0x30; k <= 0x5A; ++k) {
        if (GetAsyncKeyState(k) & 0x8000) {
            keyPressed = true;
            break;
        }
    }
    if (!keyPressed && (GetAsyncKeyState(VK_SPACE) & 0x8000 || GetAsyncKeyState(VK_BACK) & 0x8000 || GetAsyncKeyState(VK_RETURN) & 0x8000)) {
        keyPressed = true;
    }
    if (keyPressed) {
        s_keyPressCount++;
    }
    if (s_typingTimer >= 15) {
        if (s_keyPressCount >= 6) {
            if (g_currentState != STATE_CODING && g_currentState != STATE_EATING && !g_isDragging && !g_isFalling) {
                g_currentState = STATE_CODING;
                g_currentFrameIndex = 0;
                g_idleTicks = 0;
                g_happinessLevel = std::min(100, g_happinessLevel + 2);
            }
        }
        s_keyPressCount = 0;
        s_typingTimer = 0;
    }

    // 3. Music Dance Mode
    if (g_settings.musicMode) {
        float peak = GetSystemAudioPeakVolume();
        if (peak > 0.015f) {
            static int danceTicks = 0;
            danceTicks++;
            if (g_currentState != STATE_CODING && g_currentState != STATE_EATING && !g_isDragging && !g_isFalling) {
                g_currentState = (danceTicks % 2 == 0) ? STATE_ALERT : STATE_IDLE;
                g_currentFrameIndex = 0;
                g_idleTicks = 0;
            }
        }
    }

    // 4. Happiness Decay
    static int s_happinessTimer = 0;
    s_happinessTimer++;
    if (s_happinessTimer >= 300) {
        g_happinessLevel = std::max(0, g_happinessLevel - 1);
        s_happinessTimer = 0;
    }

    // Cycle frame index
    g_currentFrameIndex = (g_currentFrameIndex + 1) % GetCharacterFrameCount(g_currentState, g_settings.character);

    return (g_nekoX != oldX || g_nekoY != oldY || 
            g_currentState != oldState || g_currentFrameIndex != oldFrameIndex ||
            g_particles.size() > 0 || oldParticleCount > 0 ||
            g_isFeeding || g_isDragging || g_isFalling);
}

// Window Procedure for transparent pet window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_NCHITTEST: {
            if (g_settings.clickThrough) {
                // Click-through is active unless Ctrl is held down
                if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
                    return HTTRANSPARENT;
                }
            }
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
                Frame frame = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
                int frameIdx = frame.row * 8 + frame.col;
                if (frameIdx >= 0 && frameIdx < 48 && g_pSpriteFrames[frameIdx] && g_pSpriteFrames[frameIdx]->GetLastStatus() == Ok) {
                    Bitmap* pBmp = g_pSpriteFrames[frameIdx];
                    int relativeX = ptClient.x - catLeft;
                    int relativeY = ptClient.y - catTop;
                    
                    int pixelX = relativeX * pBmp->GetWidth() / catSize;
                    int pixelY = relativeY * pBmp->GetHeight() / catSize;
                    
                    if (pixelX >= 0 && pixelX < (int)pBmp->GetWidth() &&
                        pixelY >= 0 && pixelY < (int)pBmp->GetHeight()) {
                        Color c;
                        pBmp->GetPixel(pixelX, pixelY, &c);
                        if (c.GetAlpha() > 20) {
                            return HTCLIENT;
                        }
                    }
                }
            }
            return HTTRANSPARENT;
        }

        case WM_MOUSEWHEEL: {
            POINT pt;
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            // Disable window temporarily so WindowFromPoint passes through to the background window
            EnableWindow(hWnd, FALSE);
            HWND hwndUnder = WindowFromPoint(pt);
            EnableWindow(hWnd, TRUE);
            if (hwndUnder && hwndUnder != hWnd) {
                PostMessageW(hwndUnder, WM_MOUSEWHEEL, wParam, lParam);
            }
            break;
        }

        case WM_LBUTTONDBLCLK: {
            g_happinessLevel = std::min(100, g_happinessLevel + 15);
            PlayMeowAsync();
            int catSize = 32 * g_settings.size / 100;
            SpawnHearts(catSize / 2.0, 25.0, 10);
            g_currentState = STATE_ALERT;
            g_currentFrameIndex = 0;
            g_idleTicks = 0;
            UpdateNekoWindow();
            break;
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
            std::wstring happinessStr = L"Mutluluk: " + std::to_wstring(g_happinessLevel) + L"%";
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, happinessStr.c_str());
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
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
            if (UpdateNekoLogic()) {
                UpdateNekoWindow();
            }
            break;

        case WM_TRAY_ICON:
            if (lParam == WM_RBUTTONUP) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                
                HMENU hMenu = CreatePopupMenu();
                std::wstring happinessStr = L"Mutluluk: " + std::to_wstring(g_happinessLevel) + L"%";
                AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, happinessStr.c_str());
                AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
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
                        g_currentState = STATE_EXPECT_FOOD;
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
    wc.style = CS_DBLCLKS;
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
    for (int i = 0; i < 48; ++i) {
        if (g_pSpriteFrames[i]) {
            delete g_pSpriteFrames[i];
            g_pSpriteFrames[i] = nullptr;
        }
    }

    CloseSettingsWindow();
    GdiplusShutdown(gdiplusToken);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return (int)msg.wParam;
}
