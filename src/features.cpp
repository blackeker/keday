// features.cpp – Speech bubbles, toys, quests, achievements, events
// Bug fixes vs previous version:
//   - s_keyPressCount only lives here (not also in main/neko)
//   - ProcessClipboard is no-op safe to call when clipboard is locked
//   - Toy spawn is clamped to monitor work area
//   - PlayMeowSound removed (use audio.h PlayMeowAsync everywhere)
//   - g_catName rename uses real InputBox-style dialog
#include "features.h"
#include "settings.h"
#include "audio.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <thread>

// ── Global state ──────────────────────────────────────────────
std::wstring             g_catName         = L"Keday";
std::vector<Achievement> g_achievements;
std::vector<DailyQuest>  g_quests;
std::vector<Toy>         g_toys;
bool                     g_hologramMode    = false;
int                      g_colorTheme      = 0;
int                      g_typingSpeed     = 0;
std::wstring             g_lastClipboardText = L"";

// ── Internal state ────────────────────────────────────────────
static std::vector<std::wstring> s_quotes;
static std::vector<std::wstring> s_lowBatteryQuotes;
static std::vector<std::wstring> s_highRamQuotes;
static std::wstring              s_bubbleText     = L"";
static int                       s_bubbleLife     = 0;
static int                       s_keyPressCount  = 0;  // single source of truth
static DWORD                     s_lastWpmCheck   = 0;
static DWORD                     s_lastEventCheck = 0;
static std::wstring              s_currentGift    = L"";
static bool                      s_isHideSeek     = false;
static int                       s_hideSeekTicks  = 0;
static bool                      s_hiccupsActive  = false;
static int                       s_hiccupTicks    = 0;
static bool                      s_sleepwalking   = false;

// ── Fallback quotes ───────────────────────────────────────────
static const wchar_t* s_fallback[] = {
    L"Miyav! \u00c7ok h\u0131zl\u0131 \u00e7al\u0131\u015f\u0131yorsun! \u26a1",
    L"Bana biraz bal\u0131k verir misin? \U0001F41F",
    L"Uykum geldi... Zzz...",
    L"Clipboard'da ne kopyalad\u0131n \u00f6yle? \U0001F440",
    L"Biraz dinlensen mi? \u00c7ok oturdun.",
    L"RAM'ler leziz g\u00f6r\u00fcn\u00fcyor! \U0001F60B",
    L"Miyav! Fareyi kovalamak \u00e7ok e\u011flenceli!",
    L"Beni sevmeyi unutma! \u2665",
    L"\u015eapkam yak\u0131\u015fm\u0131\u015f m\u0131?",
    L"H\u0131\u00e7k\u0131r\u0131k tuttu! *h\u0131p* \U0001F97A",
    L"Klavyene bak\u0131yorum... Neler yaz\u0131yorsun? \U0001F914",
    L"Sessizlik huzur verir. \U0001F3B5 ...veya miyav?",
    L"Bu kadar \u00e7al\u0131\u015fmak sa\u011fl\u0131kl\u0131 m\u0131? \U0001F9D0",
    L"\u0130kindi \u00e7ay\u0131 zaman\u0131! \u2615",
    L"Pencereye bakt\u0131m, ku\u015f g\u00f6rd\u00fcm. \U0001F426",
    L"Sana sar\u0131lmak istiyorum. \U0001F90D",
};

// ── JSON loader ───────────────────────────────────────────────
static void LoadQuotesJSON() {
    s_quotes.clear();
    s_lowBatteryQuotes.clear();
    s_highRamQuotes.clear();

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t pos = dir.find_last_of(L"\\/");
    if (pos != std::wstring::npos) dir = dir.substr(0, pos);
    std::wstring jsonPath = dir + L"\\assets\\quotes.json";

    std::ifstream file(jsonPath.c_str());
    if (file.is_open()) {
        std::string line;
        int cur = 0; // 0=quotes 1=low_battery 2=high_ram
        while (std::getline(file, line)) {
            size_t a = 0;
            while ((a = line.find('"', a)) != std::string::npos) {
                size_t b = line.find('"', a + 1);
                if (b == std::string::npos) break;
                std::string val = line.substr(a + 1, b - a - 1);
                if      (val == "quotes")             cur = 0;
                else if (val == "low_battery_quotes") cur = 1;
                else if (val == "high_ram_quotes")    cur = 2;
                else if (val.length() > 2) {
                    int n = MultiByteToWideChar(CP_UTF8, 0, val.c_str(), (int)val.size(), NULL, 0);
                    std::wstring w(n, 0);
                    MultiByteToWideChar(CP_UTF8, 0, val.c_str(), (int)val.size(), &w[0], n);
                    if      (cur == 0) s_quotes.push_back(w);
                    else if (cur == 1) s_lowBatteryQuotes.push_back(w);
                    else if (cur == 2) s_highRamQuotes.push_back(w);
                }
                a = b + 1;
            }
        }
        file.close();
    }

    // Fallback if file missing / empty
    if (s_quotes.empty())
        for (const auto* q : s_fallback) s_quotes.push_back(q);
    if (s_lowBatteryQuotes.empty())
        s_lowBatteryQuotes.push_back(L"Batarya \u00e7ok azald\u0131! \U0001F50C");
    if (s_highRamQuotes.empty())
        s_highRamQuotes.push_back(L"RAM dolmak \u00fczere! \U0001F4C8");
}

// ── Init ──────────────────────────────────────────────────────
void InitFeatures() {
    LoadQuotesJSON();

    g_achievements = {
        { L"first_pat",  L"\u0130lk Sevgi",       L"Keday'i ilk defa sevdin.",       false },
        { L"toy_master", L"Oyuncak Ustas\u0131",  L"Ayn\u0131 anda 5 oyuncak kurdun.", false },
        { L"fast_typer", L"Klavye Canavar\u0131", L"Dakikada 150 tu\u015fa bast\u0131n.", false },
        { L"long_friend",L"Dostluk",              L"Keday ile 5 saat ge\u00e7irdin.", false }
    };

    g_quests = {
        { L"feed_3",      L"3 Bal\u0131k Yedir",               0, 3,  false },
        { L"play_yarn",   L"\u0130p Yuma\u011f\u0131yla Oyna", 0, 1,  false },
        { L"pat_10",      L"10 Kez Kafas\u0131n\u0131 Sev",    0, 10, false },
        { L"wpm_100",     L"Dakikada 100 Harfe Ula\u015f",     0, 1,  false },
        { L"theme_change",L"Renk Temas\u0131n\u0131 De\u011fi\u015ftir", 0, 1, false },
        { L"night_owl",   L"Gece 00:00'dan sonra 1 saat \u00e7al\u0131\u015f", 0, 1, false },
        { L"toy_5",       L"5 Oyuncak Bırak",                  0, 5,  false },
        { L"mood_happy",  L"Mutluluk 90+'a \u00e7\u0131kar",   0, 1,  false }
    };

    s_lastWpmCheck   = GetTickCount();
    s_lastEventCheck = GetTickCount();
}

void CleanupFeatures() {
    g_toys.clear();
    g_quests.clear();
    g_achievements.clear();
}

// ── Key tracking (called from neko.cpp AddKeyPress) ───────────
void AddKeyPress() {
    s_keyPressCount++;
}

// ── Break reminder ────────────────────────────────────────────
void CheckBreakReminder(HWND /*hWnd*/) {
    static DWORD s_last = 0;
    if (s_last == 0) s_last = GetTickCount();
    if (GetTickCount() - s_last > 45u * 60u * 1000u) {
        s_last = GetTickCount();
        s_bubbleText = L"45 dakikad\u0131r \u00e7al\u0131\u015f\u0131yorsun! Mola ver. \u2615";
        s_bubbleLife = 80;
        PlayMeowAsync();
    }
}

// ── Toy spawner ───────────────────────────────────────────────
void SpawnToy(ToyType type, int x, int y) {
    // Clamp to monitor work area so toy is always visible
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    GetMonitorInfoW(hMon, &mi);
    RECT& rc = mi.rcWork;
    const int sz = 24;
    x = std::max((int)rc.left, std::min(x, (int)rc.right  - sz));
    y = std::max((int)rc.top,  std::min(y, (int)rc.bottom - sz));

    Toy toy = { type, x, y, sz, true, false };
    g_toys.push_back(toy);

    if (type == TOY_BALL_OF_YARN) ProgressQuest(L"play_yarn");

    int toyCount = (int)std::count_if(g_toys.begin(), g_toys.end(),
                                      [](const Toy& t){ return t.active; });
    ProgressQuest(L"toy_5", 0); // count-based
    if (toyCount >= 5) {
        for (auto& a : g_achievements)
            if (a.id == L"toy_master") a.unlocked = true;
    }
    // toy_5 quest: completed when 5 toys placed total
    static int s_toyTotal = 0;
    s_toyTotal++;
    if (s_toyTotal >= 5) ProgressQuest(L"toy_5");
}

// ── Quest progression ─────────────────────────────────────────
void ProgressQuest(const std::wstring& id, int amount) {
    for (auto& q : g_quests) {
        if (q.id == id && !q.completed) {
            q.progress += amount;
            if (q.progress >= q.target) {
                q.progress = q.target;
                q.completed = true;
                s_bubbleText = L"G\u00f6rev Tamamland\u0131: " + q.desc + L" \U0001F389";
                s_bubbleLife = 60;
                PlayMeowAsync();
            }
        }
    }
}

// ── Mouse click handler ───────────────────────────────────────
void OnMouseClick(int /*clickX*/, int /*clickY*/) {
    // Gift pickup
    if (!s_currentGift.empty()) {
        MessageBoxW(NULL,
            (L"Keday sana bir hediye verdi:\n\n" + s_currentGift).c_str(),
            L"Keday'den Hediye! \U0001F381", MB_OK | MB_ICONINFORMATION);
        s_currentGift = L"";
        s_bubbleText  = L"Hediyeni be\u011fendin mi? Miyav! \u2665";
        s_bubbleLife  = 45;
        PlayMeowAsync();
        return;
    }

    // Random quote
    if (!s_quotes.empty()) {
        s_bubbleText = s_quotes[rand() % s_quotes.size()];
        s_bubbleLife = 50;
        PlayMeowAsync();

        ProgressQuest(L"pat_10");
        for (auto& a : g_achievements)
            if (a.id == L"first_pat" && !a.unlocked) a.unlocked = true;
    }
}

// ── Clipboard watcher (called throttled from neko.cpp) ────────
void ProcessClipboard() {
    if (!OpenClipboard(NULL)) return;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* p = static_cast<wchar_t*>(GlobalLock(hData));
        if (p) {
            std::wstring text(p);
            // Only react to new, short clipboard content
            if (text != g_lastClipboardText && text.length() > 0 && text.length() < 120) {
                g_lastClipboardText = text;
                s_bubbleText = L"Clipboard'da bir \u015feyler yakalad\u0131m! \U0001F440";
                s_bubbleLife = 30;
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

// ── Random events ─────────────────────────────────────────────
void TriggerRandomEvent() {
    int roll = rand() % 7;
    if (roll == 0) {
        s_hiccupsActive = true;
        s_hiccupTicks   = 20;
        s_bubbleText    = L"H\u0131\u00e7k\u0131r\u0131k tuttu! *h\u0131p* \U0001F97A";
        s_bubbleLife    = 30;
    } else if (roll == 1) {
        s_sleepwalking  = true;
        s_bubbleText    = L"Zzz... (Uyurgezer mod) \U0001F634";
        s_bubbleLife    = 40;
    } else if (roll == 2) {
        s_currentGift   = L"G\u00fczel bir \u00e7i\u00e7ek! \U0001F338";
        s_bubbleText    = L"Sana hediyem var! T\u0131kla! \U0001F381";
        s_bubbleLife    = 999999;
    } else if (roll == 3) {
        s_isHideSeek    = true;
        s_hideSeekTicks = 40;
        s_bubbleText    = L"Saklamba\u00e7 oynay\u0131z! \U0001F648";
        s_bubbleLife    = 35;
    } else if (roll == 4) {
        s_bubbleText    = L"Ku\u015f g\u00f6rd\u00fcm! \U0001F426 Kovalay\u0131m!";
        s_bubbleLife    = 25;
    } else if (roll == 5) {
        s_bubbleText    = L"Kelebek! \U0001F98B G\u00f6zlerim fal ta\u015f\u0131 gibi a\u00e7\u0131ld\u0131!";
        s_bubbleLife    = 25;
    } else {
        s_bubbleText    = L"Miyav miyav miyav! \U0001F63A";
        s_bubbleLife    = 20;
    }
}

// ── Main feature update (called every logic tick) ─────────────
void UpdateFeatures(double /*catX*/, double /*catY*/, int /*catSize*/) {
    // Speech bubble countdown
    if (s_bubbleLife > 0) {
        s_bubbleLife--;
        if (s_bubbleLife == 0) s_bubbleText = L"";
    }

    DWORD now = GetTickCount();

    // WPM tracker (every 10s)
    if (now - s_lastWpmCheck > 10000) {
        g_typingSpeed    = s_keyPressCount * 6;
        s_keyPressCount  = 0;
        s_lastWpmCheck   = now;

        if (g_typingSpeed >= 100) ProgressQuest(L"wpm_100");
        if (g_typingSpeed >= 150) {
            for (auto& a : g_achievements)
                if (a.id == L"fast_typer") a.unlocked = true;
        }
    }

    // System checks + random events (every 30s)
    if (now - s_lastEventCheck > 30000) {
        s_lastEventCheck = now;

        // Battery
        SYSTEM_POWER_STATUS pwr;
        if (GetSystemPowerStatus(&pwr)) {
            if (pwr.BatteryLifePercent < 20 && pwr.BatteryLifePercent != 255 &&
                !(pwr.ACLineStatus & 1) && s_bubbleLife == 0) {
                s_bubbleText = s_lowBatteryQuotes[rand() % s_lowBatteryQuotes.size()];
                s_bubbleLife = 40;
            }
        }

        // RAM
        MEMORYSTATUSEX mem; mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem) && mem.dwMemoryLoad > 85 && s_bubbleLife == 0) {
            s_bubbleText = s_highRamQuotes[rand() % s_highRamQuotes.size()];
            s_bubbleLife = 40;
        }

        // Night owl quest
        SYSTEMTIME lt; GetLocalTime(&lt);
        if (lt.wHour >= 0 && lt.wHour < 6) ProgressQuest(L"night_owl");

        // Random event (20% chance)
        if (rand() % 100 < 20 && s_bubbleLife == 0) TriggerRandomEvent();
    }
}

// ── Toy drawing ───────────────────────────────────────────────
static void DrawToyIcon(Graphics& g, ToyType type, int x, int y, int sz) {
    Pen black(Color(255,0,0,0), 1.5f);

    switch (type) {
    case TOY_BALL_OF_YARN: {
        SolidBrush red(Color(255,220,50,50));
        g.FillEllipse(&red,   x, y, sz, sz);
        g.DrawEllipse(&black, x, y, sz, sz);
        g.DrawArc(&black, Rect(x+2,  y+2,  sz-4, sz-4), 45, 180);
        g.DrawArc(&black, Rect(x+4,  y+4,  sz-8, sz-8), 135, 180);
        break;
    }
    case TOY_LASER_DOT: {
        SolidBrush glow(Color(120,255,100,100)), red(Color(255,255,0,0));
        g.FillEllipse(&glow, x-4, y-4, sz+8, sz+8);
        g.FillEllipse(&red,  x,   y,   sz,   sz);
        break;
    }
    case TOY_WATER_BOWL: {
        SolidBrush gray(Color(255,200,200,205)), blue(Color(255,80,160,240));
        g.FillEllipse(&gray, x,   y+4, sz,   sz-4);
        g.FillEllipse(&blue, x+2, y+6, sz-4, sz-8);
        g.DrawEllipse(&black,x,   y+4, sz,   sz-4);
        break;
    }
    case TOY_MOUSE_TOY: {
        SolidBrush gray(Color(255,140,140,140));
        g.FillEllipse(&gray, x+4, y+6, sz-8, sz-12);
        g.DrawLine(&black, x+sz-4, y+10, x+sz+4, y+14); // tail
        g.FillEllipse(&gray, x+2, y+4, 4, 4); // ear
        g.FillEllipse(&gray, x+8, y+4, 4, 4);
        break;
    }
    case TOY_FEATHER: {
        SolidBrush purple(Color(255,180,80,220));
        Pen stem(Color(255,120,60,30), 1.5f);
        g.DrawLine(&stem, x+sz/2, y+sz, x+sz/2, y);
        for (int i = 0; i < 5; ++i) {
            g.DrawLine(&black,
                x+sz/2, y+4+i*4,
                x+sz/2 + (i%2==0?6:-6), y+2+i*4);
        }
        break;
    }
    case TOY_BOUNCY_BALL: {
        SolidBrush ball(Color(255,60,200,100));
        g.FillEllipse(&ball, x, y, sz, sz);
        SolidBrush shine(Color(100,255,255,255));
        g.FillEllipse(&shine, x+3, y+3, sz/3, sz/3);
        break;
    }
    case TOY_CATNIP: {
        SolidBrush green(Color(255,60,180,80));
        g.FillEllipse(&green, x,     y+sz/3, sz,     2*sz/3);
        g.FillEllipse(&green, x+sz/4, y,     sz/2,   sz/2);
        break;
    }
    case TOY_FOOD_BOWL: {
        SolidBrush gray(Color(255,200,200,200)), brown(Color(255,160,100,40));
        g.FillEllipse(&gray,  x,   y+4, sz,   sz-4);
        g.FillEllipse(&brown, x+2, y+6, sz-4, sz-8);
        g.DrawEllipse(&black, x,   y+4, sz,   sz-4);
        break;
    }
    default: {
        SolidBrush box(Color(255,180,120,60));
        g.FillRectangle(&box,  x, y+4, sz, sz-8);
        g.DrawRectangle(&black,x, y+4, sz, sz-8);
        break;
    }
    }
}

// ── DrawFeatures (called from render.cpp) ─────────────────────
void DrawFeatures(Graphics& g, int catX, int catY, int catSize) {
    // 1. Speech bubble
    if (!s_bubbleText.empty()) {
        Font    font(L"Segoe UI", 9, FontStyleRegular);
        SolidBrush bg(Color(240,255,255,255)), fg(Color(255,20,20,20));
        Pen     border(Color(255,180,180,190), 1.0f);

        StringFormat fmt;
        fmt.SetAlignment(StringAlignmentCenter);
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetFormatFlags(StringFormatFlagsLineLimit);
        fmt.SetTrimming(StringTrimmingWord);

        RectF layout(0.f, 0.f, 220.f, 120.f);
        RectF bbox;
        g.MeasureString(s_bubbleText.c_str(), -1, &font, layout, &fmt, &bbox);

        float bw = std::max(bbox.Width  + 24.f, 130.f);
        float bh = std::max(bbox.Height + 16.f,  40.f);

        float bx = catX + catSize / 2.0f - bw / 2.0f;
        float by = (float)(catY - bh - 14);

        // Keep bubble on screen (horizontal clamp)
        bx = std::max(2.f, bx);

        RectF br(bx, by, bw, bh);
        g.FillRectangle(&bg, br);
        g.DrawRectangle(&border, br);

        // Tail triangle
        PointF tail[] = {
            PointF(catX + catSize/2.0f - 6.f, (float)(catY - 14)),
            PointF(catX + catSize/2.0f + 6.f, (float)(catY - 14)),
            PointF(catX + catSize/2.0f,        (float)(catY - 4))
        };
        g.FillPolygon(&bg, tail, 3);
        g.DrawLine(&border, tail[0], tail[2]);
        g.DrawLine(&border, tail[1], tail[2]);

        RectF tr(bx+8.f, by+6.f, bw-16.f, bh-12.f);
        g.DrawString(s_bubbleText.c_str(), -1, &font, tr, &fmt, &fg);
    }

    // 2. Cat name tag (subtle, small, below cat)
    if (g_catName != L"Keday" && !g_catName.empty()) {
        Font tagFont(L"Segoe UI", 7, FontStyleRegular);
        SolidBrush tagFg(Color(200, 80, 80, 80));
        g.DrawString(g_catName.c_str(), -1, &tagFont,
                     PointF((float)(catX + catSize/2 - 20), (float)(catY + catSize + 2)),
                     &tagFg);
    }

    // 3. Active toys
    for (const auto& toy : g_toys) {
        if (toy.active) {
            DrawToyIcon(g, toy.type, toy.x, toy.y, toy.size);
        }
    }
}

// ── Quest status string (for tray tooltip) ────────────────────
std::wstring GetQuestsStatusString() {
    int done = 0;
    for (const auto& q : g_quests) if (q.completed) done++;
    return L"Keday \u2013 G\u00f6revler: " +
           std::to_wstring(done) + L"/" +
           std::to_wstring(g_quests.size()) + L" Tamamland\u0131";
}
