// features.h – Speech bubbles, toys, quests, achievements, system events
#ifndef FEATURES_H
#define FEATURES_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>

using namespace Gdiplus;

// ── Achievements ──────────────────────────────────────────────
struct Achievement {
    std::wstring id;
    std::wstring title;
    std::wstring desc;
    bool         unlocked;
};

// ── Daily Quests ──────────────────────────────────────────────
struct DailyQuest {
    std::wstring id;
    std::wstring desc;
    int          progress;
    int          target;
    bool         completed;
};

// ── Toy System ────────────────────────────────────────────────
enum ToyType {
    TOY_NONE,
    TOY_BALL_OF_YARN,  // İp Yumağı
    TOY_LASER_DOT,     // Lazer Noktası
    TOY_MOUSE_TOY,     // Oyuncak Fare
    TOY_FEATHER,       // Tüy
    TOY_BOUNCY_BALL,   // Zıplayan Top
    TOY_PAPER_BAG,     // Hışırtılı Torba
    TOY_CATNIP,        // Kedi Otu
    TOY_BOX,           // Karton Kutu
    TOY_SCRATCHER,     // Tırmalama Tahtası
    TOY_TUNNEL,        // Tünel
    TOY_WATER_BOWL,    // Su Kabı
    TOY_FOOD_BOWL      // Mama Kabı
};

struct Toy {
    ToyType type;
    int     x, y;
    int     size;
    bool    active;
    bool    isDragged;
};

// ── Public API ────────────────────────────────────────────────
void InitFeatures();
void CleanupFeatures();
void UpdateFeatures(double catX, double catY, int catSize);
void DrawFeatures(Graphics& g, int catX, int catY, int catSize);
void TriggerRandomEvent();
void ProcessClipboard();
void AddKeyPress();
void CheckBreakReminder(HWND hWnd);
void OnMouseClick(int clickX, int clickY);
void SpawnToy(ToyType type, int x, int y);
void ProgressQuest(const std::wstring& questId, int amount = 1);
std::wstring GetQuestsStatusString();

// ── Shared feature state (used by render/neko) ────────────────
extern std::wstring              g_catName;
extern std::vector<Achievement>  g_achievements;
extern std::vector<DailyQuest>   g_quests;
extern std::vector<Toy>          g_toys;
extern bool                      g_hologramMode;
extern int                       g_colorTheme;   // 0:Normal 1:Sepia 2:Neon 3:Hologram
extern int                       g_typingSpeed;
extern std::wstring              g_lastClipboardText;

#endif // FEATURES_H
