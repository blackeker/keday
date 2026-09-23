// app.cpp – Definitions of all shared global state
#include "app.h"
#include <string>
#include <vector>
#include <atomic>

// ── Sprite data ───────────────────────────────────────────────
const Animation spriteSets[21] = {
    { { {3, 3} }, 1 },              // STATE_IDLE
    { { {7, 3} }, 1 },              // STATE_ALERT
    { { {5, 0}, {6, 0}, {7, 0} }, 3 }, // STATE_SCRATCH_SELF
    { { {0, 0}, {0, 1} }, 2 },      // STATE_SCRATCH_WALL_N
    { { {7, 1}, {6, 2} }, 2 },      // STATE_SCRATCH_WALL_S
    { { {2, 2}, {2, 3} }, 2 },      // STATE_SCRATCH_WALL_E
    { { {4, 0}, {4, 1} }, 2 },      // STATE_SCRATCH_WALL_W
    { { {3, 2} }, 1 },              // STATE_TIRED
    { { {2, 0}, {2, 1} }, 2 },      // STATE_SLEEPING
    { { {1, 2}, {1, 3} }, 2 },      // STATE_N
    { { {0, 2}, {0, 3} }, 2 },      // STATE_NE
    { { {3, 0}, {3, 1} }, 2 },      // STATE_E
    { { {5, 1}, {5, 2} }, 2 },      // STATE_SE
    { { {6, 3}, {7, 2} }, 2 },      // STATE_S
    { { {5, 3}, {6, 1} }, 2 },      // STATE_SW
    { { {4, 2}, {4, 3} }, 2 },      // STATE_W
    { { {1, 0}, {1, 1} }, 2 },      // STATE_NW
    { { {0, 4} }, 1 },              // STATE_EXPECT_FOOD
    { { {1, 4}, {2, 4}, {3, 4}, {4, 4} }, 4 }, // STATE_EATING
    { { {0, 5}, {1, 5}, {2, 5}, {3, 5} }, 4 }, // STATE_CODING
    { { {4, 5}, {5, 5}, {6, 5}, {7, 5} }, 4 }  // STATE_BOX
};

// ── Application globals ────────────────────────────────────────
HINSTANCE g_hInstance = NULL;
HWND      g_hMainWnd  = NULL;
Settings  g_settings;
bool      g_bVisible  = true;

Bitmap*      g_pSpriteFrames[48] = { nullptr };
std::wstring g_lastLoadedChar    = L"";

double    g_nekoX            = 100.0;
double    g_nekoY            = 100.0;
NekoState g_currentState     = STATE_IDLE;
int       g_currentFrameIndex = 0;
int       g_idleTicks         = 0;

bool   g_isDragging        = false;
POINT  g_dragStartMousePos = { 0, 0 };
POINT  g_dragStartWndPos   = { 0, 0 };
POINT  g_lastMousePos      = { 0, 0 };
bool   g_isFalling         = false;
double g_velocityX         = 0.0;
double g_velocityY         = 0.0;

std::vector<HeartParticle> g_particles;

bool   g_isFeeding = false;
double g_fishX     = 0.0;
double g_fishY     = 0.0;
int    g_feedTicks = 0;

bool   g_isWanderActive = false;
double g_wanderTargetX  = 0.0;
double g_wanderTargetY  = 0.0;

int g_happinessLevel = 80;

std::atomic<bool> g_soundPlaying{false};

// ── Shared sprite helpers ─────────────────────────────────────
std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    return dir;
}

Frame GetCharacterFrame(NekoState state, int frameIndex, const std::wstring& /*character*/) {
    const Animation& anim = spriteSets[state];
    return anim.frames[frameIndex % anim.frameCount];
}

int GetCharacterFrameCount(NekoState state, const std::wstring& /*character*/) {
    return spriteSets[state].frameCount;
}

void LoadCharacterSprite(const std::wstring& characterName) {
    for (int k = 0; k < 48; ++k) {
        if (g_pSpriteFrames[k]) {
            delete g_pSpriteFrames[k];
            g_pSpriteFrames[k] = nullptr;
        }
    }
    std::wstring baseDir = GetExeDir();
    for (int k = 0; k < 48; ++k) {
        std::wstring path = baseDir + L"\\assets\\" + characterName + L"\\" + std::to_wstring(k) + L".png";
        g_pSpriteFrames[k] = Bitmap::FromFile(path.c_str());
        if (!g_pSpriteFrames[k] || g_pSpriteFrames[k]->GetLastStatus() != Ok) {
            if (g_pSpriteFrames[k]) delete g_pSpriteFrames[k];
            std::wstring fallback = baseDir + L"\\assets\\oneko\\" + std::to_wstring(k) + L".png";
            g_pSpriteFrames[k] = Bitmap::FromFile(fallback.c_str());
            // If fallback also fails, ensure pointer is null to prevent crash
            if (g_pSpriteFrames[k] && g_pSpriteFrames[k]->GetLastStatus() != Ok) {
                delete g_pSpriteFrames[k];
                g_pSpriteFrames[k] = nullptr;
            }
        }
    }
}
