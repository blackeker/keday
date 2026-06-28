#include "features.h"
#include "settings.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <thread>

// Global states
std::wstring g_catName = L"Keday";
std::vector<Achievement> g_achievements;
std::vector<DailyQuest> g_quests;
std::vector<Toy> g_toys;
bool g_hologramMode = false;
int g_colorTheme = 0;
int g_typingSpeed = 0;
std::wstring g_lastClipboardText = L"";

// Internal feature state
static std::vector<std::wstring> s_loadedQuotes;
static std::vector<std::wstring> s_lowBatteryQuotes;
static std::vector<std::wstring> s_highRamQuotes;
static std::wstring s_activeBubbleText = L"";
static int s_bubbleLifetime = 0;
static int s_keyPressCount = 0;
static DWORD s_lastWpmCheck = 0;
static DWORD s_lastEventCheck = 0;
static std::wstring s_currentGift = L"";
static bool s_isHideAndSeek = false;
static int s_hideAndSeekTicks = 0;

// Procedural event variables
bool s_hiccupsActive = false;
int s_hiccupTicks = 0;
bool s_sleepwalking = false;

// Predefined quotes as fallback
const std::wstring s_defaultQuotes[] = {
    L"Miyav! Çok hızlı çalışıyorsun! ⚡",
    L"Bana biraz balık verir misin? 🐟",
    L"Uykum geldi... Zzz...",
    L"Clipboard'da ne kopyaladın öyle? 👀",
    L"Biraz dinlensen mi? Çok oturdun.",
    L"RAM'ler leziz görünüyor! 😋",
    L"Miyav! Fareyi kovalamak çok eğlenceli!",
    L"Beni sevmeyi unutma! ♥",
    L"Şapkam yakışmış mı?",
    L"Hıçkırık tuttu! *hıp*"
};

// Helper: Parse simple JSON strings safely without external dependencies
static void LoadQuotesJSON() {
    s_loadedQuotes.clear();
    s_lowBatteryQuotes.clear();
    s_highRamQuotes.clear();
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        dir = dir.substr(0, pos);
    }
    std::wstring jsonPath = dir + L"\\assets\\quotes.json";

    std::ifstream file(jsonPath.c_str());
    if (file.is_open()) {
        std::string line;
        int currentArray = 0; // 0 = quotes, 1 = low_battery, 2 = high_ram
        while (std::getline(file, line)) {
            size_t start = 0;
            while ((start = line.find("\"", start)) != std::string::npos) {
                size_t end = line.find("\"", start + 1);
                if (end != std::string::npos) {
                    std::string val = line.substr(start + 1, end - start - 1);
                    if (val == "quotes") {
                        currentArray = 0;
                    } else if (val == "low_battery_quotes") {
                        currentArray = 1;
                    } else if (val == "high_ram_quotes") {
                        currentArray = 2;
                    } else if (val.length() > 2) {
                        // UTF-8 to UTF-16 Conversion for Turkish characters
                        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &val[0], (int)val.size(), NULL, 0);
                        std::wstring wval(size_needed, 0);
                        MultiByteToWideChar(CP_UTF8, 0, &val[0], (int)val.size(), &wval[0], size_needed);
                        
                        if (currentArray == 0) {
                            s_loadedQuotes.push_back(wval);
                        } else if (currentArray == 1) {
                            s_lowBatteryQuotes.push_back(wval);
                        } else if (currentArray == 2) {
                            s_highRamQuotes.push_back(wval);
                        }
                    }
                    start = end + 1;
                } else {
                    break;
                }
            }
        }
        file.close();
    }
    
    if (s_loadedQuotes.empty()) {
        for (const auto& q : s_defaultQuotes) {
            s_loadedQuotes.push_back(q);
        }
    }
    if (s_lowBatteryQuotes.empty()) {
        s_lowBatteryQuotes.push_back(L"Bataryamız çok azaldı! 🔌");
    }
    if (s_highRamQuotes.empty()) {
        s_highRamQuotes.push_back(L"Sistem çok ısındı, biraz RAM temizleyelim! 📈");
    }
}

void InitFeatures() {
    LoadQuotesJSON();
    
    // Setup achievements
    g_achievements = {
        { L"first_pat", L"İlk Sevgi", L"Keday'i ilk defa sevdin.", false },
        { L"toy_master", L"Oyuncak Ustası", L"Aynı anda 5 oyuncak kurdun.", false },
        { L"fast_typer", L"Klavye Canavarı", L"Dakikada 150 tuşa bastın.", false },
        { L"long_friend", L"Dostluk", L"Keday ile 5 saat geçirdin.", false }
    };

    // Setup daily quests
    g_quests = {
        { L"feed_3", L"3 Balık Yedir", 0, 3, false },
        { L"play_yarn", L"İp Yumağıyla Oyna", 0, 1, false },
        { L"pat_10", L"10 Kez Kafasını Sev", 0, 10, false },
        { L"wpm_100", L"Dakikada 100 Harfe Ulaş", 0, 1, false },
        { L"acc_change", L"Bir Aksesuar Değiştir", 0, 1, false },
        { L"theme_change", L"Renk Temasını Değiştir", 0, 1, false }
    };

    s_lastWpmCheck = GetTickCount();
    s_lastEventCheck = GetTickCount();
}

void CleanupFeatures() {
    // Save state if needed
}

void PlayMeowSound() {
    // Generate a cute procedurally generated meow beep
    std::thread([]() {
        Beep(880, 80);
        Beep(1320, 150);
    }).detach();
}

void AddKeyPress() {
    s_keyPressCount++;
}

void CheckBreakReminder(HWND hWnd) {
    static DWORD s_lastBreakCheck = GetTickCount();
    if (GetTickCount() - s_lastBreakCheck > 45 * 60 * 1000) { // 45 minutes
        s_lastBreakCheck = GetTickCount();
        s_activeBubbleText = L"Çok fazla çalıştın! Biraz mola verelim. ☕";
        s_bubbleLifetime = 60;
        PlayMeowSound();
    }
}

void SpawnToy(ToyType type, int x, int y) {
    Toy toy = { type, x, y, 24, true, false };
    g_toys.push_back(toy);
    
    if (type == TOY_BALL_OF_YARN) {
        ProgressQuest(L"play_yarn");
    }

    if (g_toys.size() >= 5) {
        for (auto& ach : g_achievements) {
            if (ach.id == L"toy_master") ach.unlocked = true;
        }
    }
}

void ProgressQuest(const std::wstring& questId, int amount) {
    for (auto& q : g_quests) {
        if (q.id == questId && !q.completed) {
            q.progress += amount;
            if (q.progress >= q.target) {
                q.progress = q.target;
                q.completed = true;
                s_activeBubbleText = L"Görev Tamamlandı: " + q.desc + L" 🎉";
                s_bubbleLifetime = 45;
                PlayMeowSound();
            }
        }
    }
}

void OnMouseClick(int clickX, int clickY) {
    // If clicked on cat and there is a gift
    if (!s_currentGift.empty()) {
        MessageBoxW(NULL, (L"Keday sana bir hediye verdi:\n\n" + s_currentGift).c_str(), L"Keday'den Hediye! 🎁", MB_OK | MB_ICONINFORMATION);
        s_currentGift = L"";
        s_activeBubbleText = L"Hediyeni beğendin mi? Miyav! ♥";
        s_bubbleLifetime = 45;
        PlayMeowSound();
        return;
    }

    // If clicked on cat, pop a random speech bubble
    if (!s_loadedQuotes.empty()) {
        int idx = rand() % s_loadedQuotes.size();
        s_activeBubbleText = s_loadedQuotes[idx];
        s_bubbleLifetime = 45;
        PlayMeowSound();
        
        // Progress pat quest
        for (auto& q : g_quests) {
            if (q.id == L"pat_10" && !q.completed) {
                q.progress++;
                if (q.progress >= q.target) q.completed = true;
            }
        }
        for (auto& ach : g_achievements) {
            if (ach.id == L"first_pat" && !ach.unlocked) ach.unlocked = true;
        }
    }
}

void ProcessClipboard() {
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData != NULL) {
            wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pText != nullptr) {
                std::wstring text(pText);
                if (text != g_lastClipboardText && text.length() < 100) {
                    g_lastClipboardText = text;
                    s_activeBubbleText = L"Clipboard'da bir şeyler yakaladım! 👀";
                    s_bubbleLifetime = 30;
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

void TriggerRandomEvent() {
    int roll = rand() % 6;
    if (roll == 0) {
        s_hiccupsActive = true;
        s_hiccupTicks = 20;
        s_activeBubbleText = L"Hıçkırık tuttu! *hıp* 🥺";
        s_bubbleLifetime = 30;
    }
    else if (roll == 1) {
        s_sleepwalking = true;
        s_activeBubbleText = L"Zzz... (Uyurgezer mod aktif) 😴";
        s_bubbleLifetime = 40;
    }
    else if (roll == 2) {
        s_currentGift = L"Güzel bir çiçek! 🌸";
        s_activeBubbleText = L"Sana bir hediyem var! Tıkla ve al! 🎁";
        s_bubbleLifetime = 99999; // Stay active until clicked
    }
    else if (roll == 3) {
        s_isHideAndSeek = true;
        s_hideAndSeekTicks = 40;
        s_activeBubbleText = L"Hadi saklambaç oynayalım! 🙈";
        s_bubbleLifetime = 35;
    }
}

void UpdateFeatures(double catX, double catY, int catSize) {
    // 1. Update Speech Bubble Lifetime
    if (s_bubbleLifetime > 0) {
        s_bubbleLifetime--;
        if (s_bubbleLifetime == 0) s_activeBubbleText = L"";
    }

    // 2. Typing Speed WPM Tracker
    if (GetTickCount() - s_lastWpmCheck > 10000) { // Every 10 seconds
        g_typingSpeed = s_keyPressCount * 6;
        s_keyPressCount = 0;
        s_lastWpmCheck = GetTickCount();

        if (g_typingSpeed >= 100) {
            ProgressQuest(L"wpm_100");
        }

        if (g_typingSpeed >= 150) {
            for (auto& ach : g_achievements) {
                if (ach.id == L"fast_typer") ach.unlocked = true;
            }
        }
    }

    // 3. System status quotes trigger
    if (GetTickCount() - s_lastEventCheck > 30000) { // Every 30 seconds
        s_lastEventCheck = GetTickCount();
        
        // Check battery
        SYSTEM_POWER_STATUS power;
        if (GetSystemPowerStatus(&power)) {
            if (power.BatteryLifePercent < 20 && !(power.ACLineStatus & 1)) {
                if (!s_lowBatteryQuotes.empty()) {
                    s_activeBubbleText = s_lowBatteryQuotes[rand() % s_lowBatteryQuotes.size()];
                } else {
                    s_activeBubbleText = L"Bataryamız çok azaldı! 🔌";
                }
                s_bubbleLifetime = 40;
            }
        }

        // Check RAM
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            if (memInfo.dwMemoryLoad > 85) {
                if (!s_highRamQuotes.empty()) {
                    s_activeBubbleText = s_highRamQuotes[rand() % s_highRamQuotes.size()];
                } else {
                    s_activeBubbleText = L"Sistem çok ısındı, biraz RAM temizleyelim! 📈";
                }
                s_bubbleLifetime = 40;
            }
        }

        // Random events trigger (15% chance every 30s)
        if ((rand() % 100) < 15) {
            TriggerRandomEvent();
        }
    }
}

// Procedural rendering of toys
static void DrawToyIcon(Graphics& g, ToyType type, int x, int y, int size) {
    Pen blackPen(Color(255, 0, 0, 0), 1.5f);
    
    if (type == TOY_BALL_OF_YARN) {
        // Red wool ball
        SolidBrush redBrush(Color(255, 220, 50, 50));
        g.FillEllipse(&redBrush, x, y, size, size);
        g.DrawEllipse(&blackPen, x, y, size, size);
        // Thread details
        g.DrawArc(&blackPen, Rect(x + 2, y + 2, size - 4, size - 4), 45, 180);
        g.DrawArc(&blackPen, Rect(x + 4, y + 4, size - 8, size - 8), 135, 180);
    }
    else if (type == TOY_LASER_DOT) {
        // Bright glowing red dot
        SolidBrush redBrush(Color(255, 255, 0, 0));
        SolidBrush glowBrush(Color(120, 255, 100, 100));
        g.FillEllipse(&glowBrush, x - 4, y - 4, size + 8, size + 8);
        g.FillEllipse(&redBrush, x, y, size, size);
    }
    else if (type == TOY_WATER_BOWL) {
        // Water bowl
        SolidBrush blueBrush(Color(255, 80, 160, 240));
        SolidBrush grayBrush(Color(255, 200, 200, 205));
        g.FillEllipse(&grayBrush, x, y + 4, size, size - 4);
        g.FillEllipse(&blueBrush, x + 2, y + 6, size - 4, size - 8);
        g.DrawEllipse(&blackPen, x, y + 4, size, size - 4);
    }
    else if (type == TOY_MOUSE_TOY) {
        // Small toy mouse
        SolidBrush grayBrush(Color(255, 140, 140, 140));
        g.FillEllipse(&grayBrush, x + 4, y + 6, size - 8, size - 12);
        g.DrawLine(&blackPen, x + size - 4, y + 10, x + size + 4, y + 14); // Tail
        g.FillEllipse(&grayBrush, x + 2, y + 4, 4, 4); // Ear
    }
    else {
        // Generic box / treat toy icon
        SolidBrush brownBrush(Color(255, 180, 120, 60));
        g.FillRectangle(&brownBrush, x, y + 4, size, size - 8);
        g.DrawRectangle(&blackPen, x, y + 4, size, size - 8);
    }
}

void DrawFeatures(Graphics& g, int catX, int catY, int catSize) {
    // 1. Draw Speech Bubble
    if (!s_activeBubbleText.empty()) {
        Font bubbleFont(L"Segoe UI", 9, FontStyleRegular);
        SolidBrush bubbleBg(Color(240, 255, 255, 255));
        SolidBrush bubbleText(Color(255, 20, 20, 20));
        Pen borderPen(Color(255, 200, 200, 205), 1.0f);
        
        RectF bubbleRect(catX + catSize / 2 - 60, catY - 70, 130, 40);
        g.FillRectangle(&bubbleBg, bubbleRect);
        g.DrawRectangle(&borderPen, bubbleRect);
        
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        g.DrawString(s_activeBubbleText.c_str(), -1, &bubbleFont, RectF(catX + catSize / 2 - 56, catY - 66, 122, 32), &format, &bubbleText);
    }

    // 2. Draw Active Toys on Screen
    for (const auto& toy : g_toys) {
        if (toy.active) {
            DrawToyIcon(g, toy.type, toy.x, toy.y, toy.size);
        }
    }
}

std::wstring GetQuestsStatusString() {
    int completed = 0;
    for (const auto& q : g_quests) {
        if (q.completed) completed++;
    }
    return L"Keday - Görevler: " + std::to_wstring(completed) + L"/" + std::to_wstring(g_quests.size()) + L" Tamamlandı";
}
