// neko.cpp – Cat physics, AI state machine, and behavior logic
#include "neko.h"
#include "render.h"
#include "audio.h"
#include "features.h"
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <cmath>
#include <algorithm>
#include <string>

#pragma comment(lib, "ole32.lib")

// ── IAudioMeterInformation guard ─────────────────────────────
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
DEFINE_GUID(IID_IAudioMeterInformation,
    0xC02216C6, 0x8C67, 0x4B5B, 0x9D,0x00,0xD0,0x08,0xE7,0x3E,0x00,0x64);
interface DECLSPEC_UUID("C02216C6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* pfPeak) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32, float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*) = 0;
};
#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IAudioMeterInformation,
    0xC02216C6,0x8C67,0x4B5B,0x9D,0x00,0xD0,0x08,0xE7,0x3E,0x00,0x64)
#endif
#endif

static const double GRAVITY = 1.8;
static const double BOUNCE  = -0.35;

// ── Ledge detection ───────────────────────────────────────────
static bool IsDesktopClass(HWND hwnd) {
    wchar_t cls[256];
    GetClassNameW(hwnd, cls, 256);
    std::wstring s(cls);
    return s == L"Progman" || s == L"WorkerW" || s == L"Shell_TrayWnd";
}

bool IsStandingOnWindowLedge(int x, int y, int catSize, int& outLedgeY) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == g_hMainWnd || IsIconic(hwnd) || IsDesktopClass(hwnd))
        return false;
    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        int cx = x + catSize / 2;
        if (cx >= rc.left && cx <= rc.right) {
            int catBottom = y + catSize;
            if (std::abs(catBottom - rc.top) <= 3) {
                outLedgeY = rc.top;
                return true;
            }
        }
    }
    return false;
}

bool IsSpanningWindowLedge(int x, int y, double velocityY, int catSize, int& outLedgeY) {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == g_hMainWnd || IsIconic(hwnd) || IsDesktopClass(hwnd))
        return false;
    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        int cx = x + catSize / 2;
        if (cx >= rc.left && cx <= rc.right) {
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

// ── Audio peak ────────────────────────────────────────────────
float GetSystemAudioPeakVolume() {
    float peak = 0.0f;
    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    if (SUCCEEDED(hr)) {
        IMMDevice* pDevice = nullptr;
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (SUCCEEDED(hr)) {
            IAudioMeterInformation* pMeter = nullptr;
            hr = pDevice->Activate(__uuidof(IAudioMeterInformation),
                                   CLSCTX_ALL, NULL, (void**)&pMeter);
            if (SUCCEEDED(hr)) {
                pMeter->GetPeakValue(&peak);
                pMeter->Release();
            }
            pDevice->Release();
        }
        pEnum->Release();
    }
    return peak;
}

// ── Main state-machine update ─────────────────────────────────
bool UpdateNekoLogic() {
    // ── Snapshot for dirty-check ──────────────────────────────
    double    oldX           = g_nekoX;
    double    oldY           = g_nekoY;
    NekoState oldState       = g_currentState;
    int       oldFrameIndex  = g_currentFrameIndex;
    size_t    oldParticles   = g_particles.size();

    int catSize = 32 * g_settings.size / 100;

    // ── Dynamic click-through (Ctrl held = interactive) ───────
    if (g_hMainWnd) {
        LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
        LONG_PTR ns      = exStyle;
        if (g_settings.clickThrough) {
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
                ns &= ~WS_EX_TRANSPARENT;
            else
                ns |=  WS_EX_TRANSPARENT;
        } else {
            ns &= ~WS_EX_TRANSPARENT;
        }
        if (ns != exStyle) {
            SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, ns);
            SetWindowPos(g_hMainWnd, NULL, 0, 0, 0, 0,
                         SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        }
    }

    // ── Typing speed (throttled) ──────────────────────────────
    static DWORD s_lastKeyCheck = 0;
    DWORD now = GetTickCount();
    if (now - s_lastKeyCheck > 80) {
        s_lastKeyCheck = now;
        int pressed = 0;
        for (int k = 0x41; k <= 0x5A; ++k) if (GetAsyncKeyState(k) & 0x8000) pressed++;
        for (int k = 0x30; k <= 0x39; ++k) if (GetAsyncKeyState(k) & 0x8000) pressed++;
        if (GetAsyncKeyState(VK_SPACE)  & 0x8000) pressed++;
        if (GetAsyncKeyState(VK_BACK)   & 0x8000) pressed++;
        if (GetAsyncKeyState(VK_RETURN) & 0x8000) pressed++;
        if (pressed > 0) AddKeyPress();
    }

    // ── Clipboard (throttled to every 2 seconds) ──────────────
    static DWORD s_lastClipCheck = 0;
    if (now - s_lastClipCheck > 2000) {
        s_lastClipCheck = now;
        ProcessClipboard();
    }

    // ── Feature updates ───────────────────────────────────────
    UpdateFeatures(g_nekoX, g_nekoY, catSize);
    CheckBreakReminder(g_hMainWnd);

    // ── Particles update ──────────────────────────────────────
    for (auto it = g_particles.begin(); it != g_particles.end(); ) {
        it->x += it->vx;
        it->y += it->vy;
        it->vy -= 0.05;
        it->life--;
        if (it->life <= 0) it = g_particles.erase(it);
        else ++it;
    }

    // ── Feeding fish animation ────────────────────────────────
    if (g_isFeeding) {
        g_fishY += 2.0;
        int fishTargetY = catSize / 2;
        if (g_fishY >= fishTargetY) {
            g_isFeeding = false;
            g_fishY     = 0;
            g_currentState      = STATE_EATING;
            g_currentFrameIndex = 0;
            g_feedTicks         = 20;
            g_happinessLevel    = std::min(100, g_happinessLevel + 10);
            SpawnHearts((double)(catSize / 2), 20.0, 3);
            ProgressQuest(L"feed_3");
        }
    }

    if (g_feedTicks > 0) {
        g_feedTicks--;
        if (g_feedTicks == 0 && g_currentState == STATE_EATING) {
            g_currentState      = STATE_IDLE;
            g_currentFrameIndex = 0;
        }
    }

    // ── Dragging physics ──────────────────────────────────────
    if (g_isDragging) {
        POINT cur;
        GetCursorPos(&cur);
        int dx = cur.x - g_lastMousePos.x;
        int dy = cur.y - g_lastMousePos.y;
        g_velocityX  = dx * 0.5;
        g_velocityY  = dy * 0.5;
        g_lastMousePos = cur;

        g_currentFrameIndex = (g_currentFrameIndex + 1) %
            GetCharacterFrameCount(g_currentState, g_settings.character);
        return true;
    }

    // ── Falling physics ───────────────────────────────────────
    if (g_isFalling) {
        g_velocityY += GRAVITY;

        int   ledgeY = 0;
        bool  hitLedge = IsSpanningWindowLedge((int)g_nekoX, (int)g_nekoY,
                                               g_velocityY, catSize, ledgeY);
        RECT  rWork;
        GetNekoMonitorBounds((int)g_nekoX, (int)g_nekoY, rWork);
        int   floorY = rWork.bottom - catSize;

        g_nekoX += g_velocityX;
        g_nekoY += g_velocityY;

        // Clamp horizontally
        if (g_nekoX < rWork.left)              g_nekoX = rWork.left;
        if (g_nekoX > rWork.right - catSize)   g_nekoX = rWork.right - catSize;

        // Hit ledge
        if (hitLedge) {
            g_nekoY    = ledgeY - catSize;
            g_velocityY = g_velocityY * BOUNCE;
            g_velocityX *= 0.6;
            if (std::abs(g_velocityY) < 1.5) {
                g_isFalling = false;
                g_velocityX = 0;
                g_velocityY = 0;
            }
        }
        // Hit floor
        else if (g_nekoY >= floorY) {
            g_nekoY    = floorY;
            g_velocityY = g_velocityY * BOUNCE;
            g_velocityX *= 0.7;
            if (std::abs(g_velocityY) < 1.5) {
                g_isFalling = false;
                g_velocityX = 0;
                g_velocityY = 0;
            }
        }

        g_currentState      = STATE_ALERT;
        g_currentFrameIndex = (g_currentFrameIndex + 1) %
            GetCharacterFrameCount(g_currentState, g_settings.character);
        return true;
    }

    // ── Screen bounds & on-surface detection ──────────────────
    RECT rWork;
    GetNekoMonitorBounds((int)g_nekoX, (int)g_nekoY, rWork);
    int screenLeft   = rWork.left;
    int screenTop    = rWork.top;
    int screenRight  = rWork.right;
    int screenBottom = rWork.bottom;

    int  ledgeY2 = 0;
    bool onLedge = IsStandingOnWindowLedge((int)g_nekoX, (int)g_nekoY, catSize, ledgeY2);
    bool onFloor = (g_nekoY >= screenBottom - catSize - 2);

    // Gravity trigger: only when idle (not following, not wandering)
    if (!g_settings.followMouse && !g_isWanderActive && !onLedge && !onFloor) {
        g_isFalling = true;
        g_currentFrameIndex = (g_currentFrameIndex + 1) %
            GetCharacterFrameCount(g_currentState, g_settings.character);
        return true;
    }

    // ── Mouse / wander target ─────────────────────────────────
    POINT mousePos;
    GetCursorPos(&mousePos);

    double nekoCenterX = g_nekoX + catSize / 2.0;
    double nekoCenterY = g_nekoY + catSize / 2.0;

    bool shouldFollow = g_settings.followMouse;

    if (!shouldFollow) {
        if (!g_isWanderActive) {
            bool settled = onFloor || onLedge;
            // Trigger new wander: settled, idle long enough, 1/60 chance per frame
            if (settled && g_idleTicks > 30 && rand() % 60 == 0) {
                g_isWanderActive = true;
                int range = (screenRight - screenLeft - catSize);
                g_wanderTargetX = screenLeft + (range > 0 ? rand() % range : 0);
                g_wanderTargetY = onLedge ? (double)(ledgeY2 - catSize) : (double)(screenBottom - catSize);
            }
        }
        if (g_isWanderActive) shouldFollow = true;
    }

    double targetX = shouldFollow
                   ? (g_isWanderActive ? g_wanderTargetX : (double)mousePos.x)
                   : nekoCenterX;
    double targetY = shouldFollow
                   ? (g_isWanderActive ? g_wanderTargetY : (double)mousePos.y)
                   : nekoCenterY;

    double dx       = targetX - nekoCenterX;
    double dy       = targetY - nekoCenterY;
    double distance = std::sqrt(dx * dx + dy * dy);
    double speed    = std::min((double)g_settings.speed, distance * 0.5);

    // Arrived at wander target?
    if (g_isWanderActive && distance < 8.0) {
        g_isWanderActive = false;
        g_idleTicks      = 0;
        shouldFollow     = false;
    }

    // ── Idle state machine ────────────────────────────────────
    if (!shouldFollow || distance < 8.0) {
        g_idleTicks++;

        if (g_currentState >= STATE_N && g_currentState <= STATE_NW) {
            g_currentState      = STATE_IDLE;
            g_currentFrameIndex = 0;
        }

        if (g_idleTicks > 120) {
            if (g_currentState != STATE_SLEEPING && g_currentState != STATE_CODING &&
                g_currentState != STATE_BOX) {
                int r = rand() % 3;
                g_currentState      = (r == 0) ? STATE_SLEEPING :
                                      (r == 1) ? STATE_CODING   : STATE_BOX;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 40) {
            if (g_currentState != STATE_SLEEPING && g_currentState != STATE_CODING &&
                g_currentState != STATE_BOX) {
                g_currentState      = STATE_SLEEPING;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 25) {
            if (g_currentState != STATE_TIRED && g_currentState != STATE_CODING &&
                g_currentState != STATE_BOX) {
                g_currentState      = STATE_TIRED;
                g_currentFrameIndex = 0;
            }
        } else if (g_idleTicks > 12) {
            if (g_currentState != STATE_ALERT && g_currentState != STATE_SCRATCH_SELF &&
                g_currentState != STATE_CODING && g_currentState != STATE_BOX) {
                g_currentState      = (rand() % 2 == 0) ? STATE_SCRATCH_SELF : STATE_ALERT;
                g_currentFrameIndex = 0;
            }
        }
    }
    // ── Moving ────────────────────────────────────────────────
    else {
        g_idleTicks = 0;

        // Direction → animation state
        double angle = std::atan2(dy, dx) * 180.0 / 3.14159265;
        NekoState next = STATE_E;
        if      (angle >= -22.5  && angle <  22.5)  next = STATE_E;
        else if (angle >=  22.5  && angle <  67.5)  next = STATE_SE;
        else if (angle >=  67.5  && angle < 112.5)  next = STATE_S;
        else if (angle >= 112.5  && angle < 157.5)  next = STATE_SW;
        else if (angle >= 157.5  || angle < -157.5) next = STATE_W;
        else if (angle >= -157.5 && angle < -112.5) next = STATE_NW;
        else if (angle >= -112.5 && angle <  -67.5) next = STATE_N;
        else if (angle >=  -67.5 && angle <  -22.5) next = STATE_NE;
        g_currentState = next;

        double moveX = (dx / distance) * speed;
        double moveY = (dy / distance) * speed;
        g_nekoX += moveX;
        g_nekoY += moveY;

        // Clamp to monitor
        if (g_nekoX < screenLeft)              g_nekoX = screenLeft;
        if (g_nekoY < screenTop)               g_nekoY = screenTop;
        if (g_nekoX > screenRight  - catSize)  g_nekoX = screenRight  - catSize;
        if (g_nekoY > screenBottom - catSize)  g_nekoY = screenBottom - catSize;

        // Edge scratch when bumping walls
        const int ET = 5;
        if      (g_nekoY <= screenTop    + ET) g_currentState = STATE_SCRATCH_WALL_N;
        else if (g_nekoY >= screenBottom - catSize - ET) g_currentState = STATE_SCRATCH_WALL_S;
        else if (g_nekoX <= screenLeft   + ET) g_currentState = STATE_SCRATCH_WALL_W;
        else if (g_nekoX >= screenRight  - catSize - ET) g_currentState = STATE_SCRATCH_WALL_E;
    }

    // ── Zzz particles ─────────────────────────────────────────
    if (g_currentState == STATE_SLEEPING && rand() % 12 == 0)
        SpawnHearts(catSize / 2.0, 20.0, 1, L'Z');

    // ── Keyboard → CODING state ───────────────────────────────
    {
        static int  s_keyCount   = 0;
        static int  s_keyTimer   = 0;
        bool anyKey = false;
        for (int k = 0x30; k <= 0x5A; ++k)
            if (GetAsyncKeyState(k) & 0x8000) { anyKey = true; break; }
        if (!anyKey)
            anyKey = (GetAsyncKeyState(VK_SPACE)  & 0x8000) ||
                     (GetAsyncKeyState(VK_BACK)   & 0x8000) ||
                     (GetAsyncKeyState(VK_RETURN) & 0x8000);
        if (anyKey) s_keyCount++;
        s_keyTimer++;
        if (s_keyTimer >= 15) {
            if (s_keyCount >= 6 && g_currentState != STATE_CODING &&
                g_currentState != STATE_EATING && !g_isDragging && !g_isFalling) {
                g_currentState      = STATE_CODING;
                g_currentFrameIndex = 0;
                g_idleTicks         = 0;
                g_happinessLevel    = std::min(100, g_happinessLevel + 2);
            }
            s_keyCount = 0;
            s_keyTimer = 0;
        }
    }

    // ── Music dance mode ──────────────────────────────────────
    if (g_settings.musicMode) {
        float peak = GetSystemAudioPeakVolume();
        if (peak > 0.015f) {
            static int danceTicks = 0;
            danceTicks++;
            if (g_currentState != STATE_CODING && g_currentState != STATE_EATING &&
                !g_isDragging && !g_isFalling) {
                g_currentState      = (danceTicks % 2 == 0) ? STATE_ALERT : STATE_IDLE;
                g_currentFrameIndex = 0;
                g_idleTicks         = 0;
            }
        }
    }

    // ── Happiness decay ───────────────────────────────────────
    static int s_happTimer = 0;
    s_happTimer++;
    if (s_happTimer >= 300) {
        g_happinessLevel = std::max(0, g_happinessLevel - 1);
        s_happTimer      = 0;
    }

    // ── Advance animation frame ───────────────────────────────
    g_currentFrameIndex = (g_currentFrameIndex + 1) %
        GetCharacterFrameCount(g_currentState, g_settings.character);

    return (g_nekoX != oldX || g_nekoY != oldY ||
            g_currentState != oldState || g_currentFrameIndex != oldFrameIndex ||
            g_particles.size() > 0 || oldParticles > 0 ||
            g_isFeeding || g_isDragging || g_isFalling);
}
