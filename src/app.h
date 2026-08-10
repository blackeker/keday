// app.h – Shared global application state for Keday
// All modules include this instead of declaring globals themselves.
#ifndef APP_H
#define APP_H

#ifndef WINVER
#define WINVER 0x0600
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <atomic>
#include "settings.h"

using namespace Gdiplus;

// ── Menu IDs ────────────────────────────────────────────────
#define WM_TRAY_ICON          (WM_USER + 1)
#define IDM_SETTINGS          2001
#define IDM_EXIT              2002
#define IDM_TOGGLE_VISIBLE    2003
#define IDM_PET               2004
#define IDM_FEED              2005
#define IDM_THEME_NORMAL      3001
#define IDM_THEME_SEPIA       3002
#define IDM_THEME_NEON        3003
#define IDM_THEME_HOLOGRAM    3004
#define IDM_TOY_YARN          3005
#define IDM_TOY_LASER         3006
#define IDM_TOY_MOUSE         3007
#define IDM_TOY_WATER         3008
#define IDM_RENAME            3009

// ── Neko States ─────────────────────────────────────────────
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

// ── Sprite / Animation data ──────────────────────────────────
struct Frame {
    int col;
    int row;
};

struct Animation {
    Frame frames[4];
    int frameCount;
};

extern const Animation spriteSets[21]; // defined in render.cpp

// ── Particle system ──────────────────────────────────────────
struct HeartParticle {
    double x, y;
    double vx, vy;
    int life;
    wchar_t symbol = L'\x2665'; // ♥
};

// ── Application globals (defined in app.cpp) ─────────────────
extern HINSTANCE g_hInstance;
extern HWND      g_hMainWnd;
extern Settings  g_settings;
extern bool      g_bVisible;

// Sprite cache
extern Bitmap* g_pSpriteFrames[48];
extern std::wstring g_lastLoadedChar;

// Neko position & state
extern double    g_nekoX;
extern double    g_nekoY;
extern NekoState g_currentState;
extern int       g_currentFrameIndex;
extern int       g_idleTicks;

// Physics
extern bool   g_isDragging;
extern POINT  g_dragStartMousePos;
extern POINT  g_dragStartWndPos;
extern POINT  g_lastMousePos;
extern bool   g_isFalling;
extern double g_velocityX;
extern double g_velocityY;

// Particles
extern std::vector<HeartParticle> g_particles;

// Feeding
extern bool   g_isFeeding;
extern double g_fishX;
extern double g_fishY;
extern int    g_feedTicks;

// Wandering
extern bool   g_isWanderActive;
extern double g_wanderTargetX;
extern double g_wanderTargetY;

// Happiness
extern int g_happinessLevel;

// Sound gate
extern std::atomic<bool> g_soundPlaying;

// ── Shared helper functions ───────────────────────────────────
// Sprite access
Frame       GetCharacterFrame(NekoState state, int frameIndex, const std::wstring& character);
int         GetCharacterFrameCount(NekoState state, const std::wstring& character);
void        LoadCharacterSprite(const std::wstring& characterName);

// Utility
std::wstring GetExeDir();
void         ApplyWindowSettings();

#endif // APP_H
