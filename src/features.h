#ifndef FEATURES_H
#define FEATURES_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>

using namespace Gdiplus;

// 1. Achievements & Daily Quests
struct Achievement {
    std::wstring id;
    std::wstring title;
    std::wstring desc;
    bool unlocked;
};

struct DailyQuest {
    std::wstring id;
    std::wstring desc;
    int progress;
    int target;
    bool completed;
};

// 2. Toy System (10+ toys & Water bowl)
enum ToyType {
    TOY_NONE,
    TOY_BALL_OF_YARN, // Ip Yumagi
    TOY_LASER_DOT,    // Lazer Noktasi
    TOY_MOUSE_TOY,    // Fare Oyuncak
    TOY_FEATHER,      // Tuy
    TOY_BOUNCY_BALL,  // Ziplayan Top
    TOY_PAPER_BAG,    // Hisir Torba
    TOY_CATNIP,       // Kedi Otu
    TOY_BOX,          // Karton Kutu
    TOY_SCRATCHER,    // Tirmalama Tahtasi
    TOY_TUNNEL,       // Tunel
    TOY_WATER_BOWL,   // Su Kabi
    TOY_FOOD_BOWL     // Mama Kabi
};

struct Toy {
    ToyType type;
    int x, y;
    int size;
    bool active;
    bool isDragged;
};

// Functions declared for features
void InitFeatures();
void CleanupFeatures();
void UpdateFeatures(double catX, double catY, int catSize);
void DrawFeatures(Graphics& g, int catX, int catY, int catSize);
void TriggerRandomEvent();
void ProcessClipboard();
void AddKeyPress();
void CheckBreakReminder(HWND hWnd);
void PlayMeowSound();
void OnMouseClick(int clickX, int clickY);
void SpawnToy(ToyType type, int x, int y);
void ProgressQuest(const std::wstring& questId, int amount = 1);
std::wstring GetQuestsStatusString();

// Global settings/states for features
extern std::wstring g_catName;
extern std::vector<Achievement> g_achievements;
extern std::vector<DailyQuest> g_quests;
extern std::vector<Toy> g_toys;
extern bool g_hologramMode;
extern int g_colorTheme; // 0: None, 1: Sepia, 2: Neon, 3: Hologram
extern int g_typingSpeed; // Characters typed in last minute
extern std::wstring g_lastClipboardText;

#endif // FEATURES_H
