#include "settings_window.h"
#include "resources.h"
#include <gdiplus.h>
#include <shellapi.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace Gdiplus;

// Defined in main.cpp
extern std::wstring GetExeDir();

static HWND g_hSettingsWnd = NULL;
static Settings* g_pSettings = nullptr;
static void (*g_onSettingsChanged)() = nullptr;

enum SliderType {
    SLIDER_SPEED,
    SLIDER_SIZE,
    SLIDER_OPACITY,
    SLIDER_VOLUME
};

struct Card {
    Rect rect;
    std::wstring name;
    std::wstring label;
    Bitmap* previewFrames[2];
};

struct Slider {
    Rect trackRect;
    int* valPtr;
    int minVal;
    int maxVal;
    std::wstring label;
    SliderType type;
    bool isDragging;
};

struct Toggle {
    Rect rect;
    bool* valPtr;
    std::wstring label;
};

struct ButtonControl {
    Rect rect;
    std::wstring label;
    std::wstring url;
    Color color1;
    Color color2;
};

struct AccessoryBtn {
    Rect rect;
    int value;
    std::wstring label;
};

enum HoverType {
    HOVER_NONE,
    HOVER_CARD,
    HOVER_SLIDER,
    HOVER_TOGGLE,
    HOVER_SOCIAL,
    HOVER_ACCESSORY,
    HOVER_OK_BTN,
    HOVER_CANCEL_BTN
};

struct SettingsWindowState {
    Settings originalSettings;
    std::vector<Card> charCards;
    std::vector<Slider> sliders;
    std::vector<Toggle> toggles;
    std::vector<ButtonControl> socialButtons;
    std::vector<AccessoryBtn> accessoryBtns;
    Rect okBtnRect;
    Rect cancelBtnRect;
    HoverType hoverType = HOVER_NONE;
    int hoverIndex = -1;
    bool trackingMouse = false;
    int previewFrameIndex = 0;
    int scrollY = 0;
    int maxScrollY = 605;
    int focusedSliderIdx = -1;
};

static SettingsWindowState* g_pState = nullptr;

// Helper to draw rounded rectangles in GDI+
static void AddRoundRectToPath(GraphicsPath& path, Rect rect, int radius) {
    int diameter = radius * 2;
    if (diameter > rect.Width) diameter = rect.Width;
    if (diameter > rect.Height) diameter = rect.Height;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270, 90);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0, 90);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90, 90);
    path.CloseFigure();
}

static void FillRoundRect(Graphics& g, Brush* brush, Rect rect, int radius) {
    GraphicsPath path;
    AddRoundRectToPath(path, rect, radius);
    g.FillPath(brush, &path);
}

static void DrawRoundRect(Graphics& g, Pen* pen, Rect rect, int radius) {
    GraphicsPath path;
    AddRoundRectToPath(path, rect, radius);
    g.DrawPath(pen, &path);
}

const int WIN_WIDTH = 450;
const int WIN_HEIGHT = 650;

static void InitializeLayout(HWND hWnd) {
    if (!g_pState) return;

    g_pState->charCards.clear();
    g_pState->sliders.clear();
    g_pState->toggles.clear();
    g_pState->socialButtons.clear();
    g_pState->accessoryBtns.clear();
    
    g_pState->maxScrollY = 605;

    // 1. Character Cards
    std::wstring chars[] = { L"oneko", L"red", L"green", L"rw", L"dog", L"tora", L"sakura", L"bsd", L"tomoyo", L"eevee", L"bunny", L"black" };
    std::wstring charLabels[] = { L"Keday", L"Kırmızı", L"Yeşil", L"Siyah-Beyaz", L"Köpek", L"Kaplan", L"Sakura", L"BSD Daemon", L"Tomoyo", L"Eevee", L"Tavşan", L"Kara Kedi" };
    for (int i = 0; i < 12; ++i) {
        Card card;
        card.rect = Rect(35 + (i % 4) * 98, 90 + (i / 4) * 85, 72, 72);
        card.name = chars[i];
        card.label = charLabels[i];
        std::wstring baseDir = GetExeDir();
        std::wstring folderPath = baseDir + L"\\assets\\" + chars[i];
        card.previewFrames[0] = Bitmap::FromFile((folderPath + L"\\3.png").c_str());
        card.previewFrames[1] = Bitmap::FromFile((folderPath + L"\\4.png").c_str());
        g_pState->charCards.push_back(card);
    }

    // 2. Sliders
    if (g_pSettings) {
        Slider speed;
        speed.trackRect = Rect(30, 380, 390, 8);
        speed.valPtr = &g_pSettings->speed;
        speed.minVal = 1;
        speed.maxVal = 30;
        speed.label = L"Yürüme Hızı";
        speed.type = SLIDER_SPEED;
        speed.isDragging = false;
        g_pState->sliders.push_back(speed);

        Slider size;
        size.trackRect = Rect(30, 435, 390, 8);
        size.valPtr = &g_pSettings->size;
        size.minVal = 50;
        size.maxVal = 300;
        size.label = L"Karakter Boyutu";
        size.type = SLIDER_SIZE;
        size.isDragging = false;
        g_pState->sliders.push_back(size);

        Slider opacity;
        opacity.trackRect = Rect(30, 490, 390, 8);
        opacity.valPtr = &g_pSettings->opacity;
        opacity.minVal = 10;
        opacity.maxVal = 100;
        opacity.label = L"Şeffaflık";
        opacity.type = SLIDER_OPACITY;
        opacity.isDragging = false;
        g_pState->sliders.push_back(opacity);

        Slider volume;
        volume.trackRect = Rect(30, 545, 390, 8);
        volume.valPtr = &g_pSettings->volume;
        volume.minVal = 0;
        volume.maxVal = 100;
        volume.label = L"Ses Düzeyi";
        volume.type = SLIDER_VOLUME;
        volume.isDragging = false;
        g_pState->sliders.push_back(volume);

        // 3. Toggles (2 columns, starting at Y = 605)
        Toggle t1 = { Rect(30, 605, 180, 30), &g_pSettings->alwaysOnTop, L"Her Zaman Üstte" };
        Toggle t2 = { Rect(30, 645, 180, 30), &g_pSettings->followMouse, L"Fareyi Takip Et" };
        Toggle t3 = { Rect(30, 685, 180, 30), &g_pSettings->showShadow, L"Gölge Göster" };
        Toggle t4 = { Rect(30, 725, 180, 30), &g_pSettings->isDarkMode, L"Koyu Tema" };
        Toggle t5 = { Rect(30, 765, 180, 30), &g_pSettings->clickThrough, L"Tıklamayı Geçir" };
        
        Toggle t6 = { Rect(240, 605, 180, 30), &g_pSettings->minimizeToTray, L"Tepsiye Küçült" };
        Toggle t7 = { Rect(240, 645, 180, 30), &g_pSettings->autoLaunch, L"Başlangıçta Aç" };
        Toggle t8 = { Rect(240, 685, 180, 30), &g_pSettings->hideOnFullscreen, L"Tam Ekran Gizle" };
        Toggle t9 = { Rect(240, 725, 180, 30), &g_pSettings->musicMode, L"Müzik Dans Modu" };

        g_pState->toggles.push_back(t1);
        g_pState->toggles.push_back(t2);
        g_pState->toggles.push_back(t3);
        g_pState->toggles.push_back(t4);
        g_pState->toggles.push_back(t5);
        g_pState->toggles.push_back(t6);
        g_pState->toggles.push_back(t7);
        g_pState->toggles.push_back(t8);
        g_pState->toggles.push_back(t9);
    }

    // 4. Accessory Buttons (None, Glasses, Santa Hat, Bow Tie at Y = 845)
    AccessoryBtn a0 = { Rect(30, 845, 90, 36), 0, L"Yok" };
    AccessoryBtn a1 = { Rect(130, 845, 90, 36), 1, L"Gözlük" };
    AccessoryBtn a2 = { Rect(230, 845, 90, 36), 2, L"Şapka" };
    AccessoryBtn a3 = { Rect(330, 845, 90, 36), 3, L"Papyon" };
    g_pState->accessoryBtns.push_back(a0);
    g_pState->accessoryBtns.push_back(a1);
    g_pState->accessoryBtns.push_back(a2);
    g_pState->accessoryBtns.push_back(a3);

    // 5. Social Buttons (Shifted down)
    ButtonControl discord = { Rect(30, 905, 115, 36), L"Discord", L"https://discord.gg/hentaitr", Color(255, 114, 137, 218), Color(255, 91, 110, 174) };
    ButtonControl github = { Rect(167, 905, 115, 36), L"GitHub", L"https://github.com/blackeker/Keday", Color(255, 51, 51, 51), Color(255, 36, 41, 46) };
    ButtonControl insta = { Rect(305, 905, 115, 36), L"Instagram", L"https://instagram.com/blackekerr", Color(255, 240, 148, 51), Color(255, 188, 24, 136) };
    g_pState->socialButtons.push_back(discord);
    g_pState->socialButtons.push_back(github);
    g_pState->socialButtons.push_back(insta);

    // 6. Footer Buttons
    g_pState->cancelBtnRect = Rect(30, WIN_HEIGHT - 50, 180, 40);
    g_pState->okBtnRect = Rect(240, WIN_HEIGHT - 50, 180, 40);
}

static void DrawAccessoryPreview(Graphics& g, int x, int y, int size, int type) {
    if (type <= 0) return;
    if (type == 1) {
        Pen glassesPen(Color(255, 0, 0, 0), size / 20.0f);
        g.DrawRectangle(&glassesPen, x + (int)(size * 0.32), y + (int)(size * 0.38), (int)(size * 0.16), (int)(size * 0.12));
        g.DrawRectangle(&glassesPen, x + (int)(size * 0.52), y + (int)(size * 0.38), (int)(size * 0.16), (int)(size * 0.12));
        g.DrawLine(&glassesPen, x + (int)(size * 0.48), y + (int)(size * 0.44), x + (int)(size * 0.52), y + (int)(size * 0.44));
    } 
    else if (type == 2) {
        SolidBrush redBrush(Color(255, 230, 40, 40));
        SolidBrush whiteBrush(Color(255, 245, 245, 245));
        Point points[] = {
            Point(x + (int)(size * 0.35), y + (int)(size * 0.22)),
            Point(x + (int)(size * 0.65), y + (int)(size * 0.22)),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.04))
        };
        g.FillPolygon(&redBrush, points, 3);
        g.FillRectangle(&whiteBrush, x + (int)(size * 0.30), y + (int)(size * 0.20), (int)(size * 0.40), (int)(size * 0.06));
        g.FillEllipse(&whiteBrush, x + (int)(size * 0.46), y + (int)(size * 0.01), (int)(size * 0.08), (int)(size * 0.08));
    } 
    else if (type == 3) {
        SolidBrush redBrush(Color(255, 220, 20, 60));
        SolidBrush centerBrush(Color(255, 150, 10, 40));
        Point leftWing[] = {
            Point(x + (int)(size * 0.38), y + (int)(size * 0.56)),
            Point(x + (int)(size * 0.38), y + (int)(size * 0.68)),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.62))
        };
        g.FillPolygon(&redBrush, leftWing, 3);
        Point rightWing[] = {
            Point(x + (int)(size * 0.62), y + (int)(size * 0.56)),
            Point(x + (int)(size * 0.62), y + (int)(size * 0.68)),
            Point(x + (int)(size * 0.50), y + (int)(size * 0.62))
        };
        g.FillPolygon(&redBrush, rightWing, 3);
        g.FillEllipse(&centerBrush, x + (int)(size * 0.47), y + (int)(size * 0.59), (int)(size * 0.06), (int)(size * 0.06));
    }
}

static void PaintSettings(HWND hWnd, HDC hdc) {
    if (!g_pState || !g_pSettings) return;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    // Create Double Buffer DC
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    Graphics g(hMemDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Theme Color Selection (Premium Dark & Light Palette)
    bool dark = g_pSettings->isDarkMode;
    Color bgCol = dark ? Color(255, 13, 14, 18) : Color(255, 246, 247, 250);
    Color textCol = dark ? Color(255, 245, 246, 250) : Color(255, 25, 27, 38);
    Color subTextCol = dark ? Color(255, 150, 155, 175) : Color(255, 110, 115, 130);
    Color cardBgCol = dark ? Color(255, 24, 25, 32) : Color(255, 255, 255, 255);
    Color borderCol = dark ? Color(255, 36, 37, 48) : Color(255, 222, 226, 235);
    
    // Gradient active colors (Vibrant Pink theme)
    Color activeCol1 = Color(255, 255, 80, 130);
    Color activeCol2 = Color(255, 200, 30, 80);

    SolidBrush bgBrush(bgCol);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 20, FontStyleBold, UnitPoint);
    Font subFont(&fontFamily, 9, FontStyleRegular, UnitPoint);
    Font sectionFont(&fontFamily, 10, FontStyleBold, UnitPoint);
    Font labelFont(&fontFamily, 9.5f, FontStyleRegular, UnitPoint);
    Font valueFont(&fontFamily, 9, FontStyleBold, UnitPoint);
    Font buttonFont(&fontFamily, 9.5f, FontStyleBold, UnitPoint);

    SolidBrush textBrush(textCol);
    SolidBrush subTextBrush(subTextCol);
    SolidBrush activeBrush(activeCol1);

    // Header Drawing
    g.DrawString(L"Keday Ayarları", -1, &titleFont, PointF(30, 20), &textBrush);
    g.DrawString(L"Masaüstü kedisi yapılandırma paneli", -1, &subFont, PointF(30, 52), &subTextBrush);
    Pen dividerPen(borderCol, 1);
    g.DrawLine(&dividerPen, 30, 75, 420, 75);

    // Apply scroll translation
    g.TranslateTransform(0, -(float)g_pState->scrollY);

    // Draw Character Cards
    for (size_t i = 0; i < g_pState->charCards.size(); ++i) {
        const auto& card = g_pState->charCards[i];
        bool isSelected = (g_pSettings->character == card.name);
        bool isHovered = (g_pState->hoverType == HOVER_CARD && g_pState->hoverIndex == (int)i);
        
        Color currentCardBg = isSelected ? (dark ? Color(255, 32, 33, 44) : Color(255, 250, 240, 245)) : (isHovered ? (dark ? Color(255, 30, 31, 38) : Color(255, 245, 247, 250)) : cardBgCol);
        SolidBrush cardBrush(currentCardBg);
        FillRoundRect(g, &cardBrush, card.rect, 12);

        if (isSelected) {
            LinearGradientBrush activeGradBorder(card.rect, activeCol1, activeCol2, LinearGradientModeVertical);
            Pen activePen(&activeGradBorder, 2);
            DrawRoundRect(g, &activePen, card.rect, 12);
        } else {
            Pen borderPen(isHovered ? (dark ? Color(255, 80, 82, 105) : Color(255, 255, 110, 140)) : borderCol, 1);
            DrawRoundRect(g, &borderPen, card.rect, 12);
        }

        // Draw animated character preview
        int frameSelect = g_pState->previewFrameIndex % 2;
        Bitmap* pBmp = card.previewFrames[frameSelect];
        if (pBmp && pBmp->GetLastStatus() == Ok) {
            int cx = card.rect.X + (card.rect.Width - 40)/2;
            int cy = card.rect.Y + 6;
            g.DrawImage(pBmp, Rect(cx, cy, 40, 40), 0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel);
            if (isSelected) {
                DrawAccessoryPreview(g, cx, cy, 40, g_pSettings->accessory);
            }
        }

        // Draw label
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        RectF textRect((REAL)card.rect.X, (REAL)card.rect.Y + 50, (REAL)card.rect.Width, 20.0f);
        g.DrawString(card.label.c_str(), -1, &subFont, textRect, &format, &textBrush);
    }

    // Draw Sliders
    for (size_t i = 0; i < g_pState->sliders.size(); ++i) {
        const auto& slider = g_pState->sliders[i];
        g.DrawString(slider.label.c_str(), -1, &sectionFont, PointF((REAL)slider.trackRect.X, (REAL)slider.trackRect.Y - 22), &textBrush);

        // Track background
        SolidBrush trackBrush(dark ? Color(255, 36, 37, 48) : Color(255, 230, 234, 240));
        FillRoundRect(g, &trackBrush, slider.trackRect, 4);

        // Active part using Gradient
        int currentVal = *(slider.valPtr);
        float percent = (float)(currentVal - slider.minVal) / (slider.maxVal - slider.minVal);
        int activeWidth = (int)(slider.trackRect.Width * percent);
        if (activeWidth > 0) {
            Rect activeRect(slider.trackRect.X, slider.trackRect.Y, activeWidth, slider.trackRect.Height);
            LinearGradientBrush activeGrad(activeRect, activeCol1, activeCol2, LinearGradientModeHorizontal);
            FillRoundRect(g, &activeGrad, activeRect, 4);
        }

        // Focused slider highlight
        if (g_pState->focusedSliderIdx == (int)i) {
            Pen focusPen(activeCol1, 1.2f);
            focusPen.SetDashStyle(DashStyleDash);
            Rect focusRect(slider.trackRect.X - 4, slider.trackRect.Y - 4, slider.trackRect.Width + 8, slider.trackRect.Height + 8);
            g.DrawRectangle(&focusPen, focusRect);
        }

        // Thumb handle
        int thumbX = slider.trackRect.X + activeWidth;
        int thumbY = slider.trackRect.Y + slider.trackRect.Height / 2;
        bool isSliderHovered = (g_pState->hoverType == HOVER_SLIDER && g_pState->hoverIndex == (int)i);
        int thumbRadius = isSliderHovered ? 10 : 8;

        // Shadow for thumb
        SolidBrush thumbShadow(Color(40, 0, 0, 0));
        g.FillEllipse(&thumbShadow, thumbX - thumbRadius + 1, thumbY - thumbRadius + 2, thumbRadius * 2, thumbRadius * 2);

        SolidBrush thumbBrush(Color(255, 255, 255, 255));
        g.FillEllipse(&thumbBrush, thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        
        Pen thumbBorderPen(isSliderHovered ? activeCol1 : (dark ? activeCol2 : Color(255, 255, 75, 110)), 2.0f);
        g.DrawEllipse(&thumbBorderPen, thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);

        // Value display text
        std::wstring valText;
        if (slider.type == SLIDER_OPACITY) valText = L"%" + std::to_wstring(currentVal);
        else if (slider.type == SLIDER_SIZE) valText = L"%" + std::to_wstring(currentVal);
        else valText = std::to_wstring(currentVal);

        StringFormat valFormat;
        valFormat.SetAlignment(StringAlignmentFar);
        RectF valRect((REAL)slider.trackRect.X, (REAL)slider.trackRect.Y - 22, (REAL)slider.trackRect.Width, 20.0f);
        g.DrawString(valText.c_str(), -1, &valueFont, valRect, &valFormat, &activeBrush);
    }

    // Draw Toggles
    for (size_t i = 0; i < g_pState->toggles.size(); ++i) {
        const auto& toggle = g_pState->toggles[i];
        bool state = *(toggle.valPtr);
        bool isToggleHovered = (g_pState->hoverType == HOVER_TOGGLE && g_pState->hoverIndex == (int)i);

        // Switch box (size 40x20 at right of toggle rect)
        Rect switchRect(toggle.rect.X + toggle.rect.Width - 40, toggle.rect.Y + 5, 40, 20);
        if (state) {
            LinearGradientBrush switchBg(switchRect, activeCol1, activeCol2, LinearGradientModeHorizontal);
            FillRoundRect(g, &switchBg, switchRect, 10);
        } else {
            Color toggleBgColor = isToggleHovered ? (dark ? Color(255, 42, 43, 52) : Color(255, 235, 238, 243)) : (dark ? Color(255, 30, 31, 38) : Color(255, 240, 242, 245));
            SolidBrush switchBg(toggleBgColor);
            FillRoundRect(g, &switchBg, switchRect, 10);
            Pen switchBorder(borderCol, 1);
            DrawRoundRect(g, &switchBorder, switchRect, 10);
        }

        // Switch knob
        SolidBrush knobBrush(Color(255, 255, 255, 255));
        int knobX = state ? (switchRect.X + 22) : (switchRect.X + 2);
        g.FillEllipse(&knobBrush, knobX, switchRect.Y + 2, 16, 16);

        if (!state) {
            Pen knobBorder(dark ? Color(255, 80, 80, 85) : Color(255, 200, 200, 205), 0.5f);
            g.DrawEllipse(&knobBorder, knobX, switchRect.Y + 2, 16, 16);
        }

        // Label
        g.DrawString(toggle.label.c_str(), -1, &labelFont, PointF((REAL)toggle.rect.X, (REAL)toggle.rect.Y + 5), &textBrush);
        
        // Hover highlight background for toggle row
        if (isToggleHovered) {
            Color rowHoverColor = dark ? Color(15, 255, 255, 255) : Color(8, 0, 0, 0);
            SolidBrush rowHoverBrush(rowHoverColor);
            FillRoundRect(g, &rowHoverBrush, toggle.rect, 6);
        }
    }

    // Draw Accessory Buttons Section
    g.DrawString(L"Aksesuar Seçimi", -1, &sectionFont, PointF(30, 820), &textBrush);
    for (size_t i = 0; i < g_pState->accessoryBtns.size(); ++i) {
        const auto& btn = g_pState->accessoryBtns[i];
        bool isSelected = (g_pSettings->accessory == btn.value);
        bool isHovered = (g_pState->hoverType == HOVER_ACCESSORY && g_pState->hoverIndex == (int)i);
        
        Color btnBgCol = isSelected ? (dark ? Color(255, 32, 33, 44) : Color(255, 250, 240, 245)) : (isHovered ? (dark ? Color(255, 30, 31, 38) : Color(255, 245, 247, 250)) : cardBgCol);
        SolidBrush btnBrush(btnBgCol);
        FillRoundRect(g, &btnBrush, btn.rect, 10);
        
        if (isSelected) {
            LinearGradientBrush activeGradBorder(btn.rect, activeCol1, activeCol2, LinearGradientModeVertical);
            Pen activePen(&activeGradBorder, 2);
            DrawRoundRect(g, &activePen, btn.rect, 10);
        } else {
            Pen borderPen(isHovered ? activeCol1 : borderCol, 1);
            DrawRoundRect(g, &borderPen, btn.rect, 10);
        }
        
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        RectF btnRectF((REAL)btn.rect.X, (REAL)btn.rect.Y, (REAL)btn.rect.Width, (REAL)btn.rect.Height);
        g.DrawString(btn.label.c_str(), -1, &buttonFont, btnRectF, &format, &textBrush);
    }

    // Draw Social Buttons
    for (size_t i = 0; i < g_pState->socialButtons.size(); ++i) {
        const auto& btn = g_pState->socialButtons[i];
        bool isBtnHovered = (g_pState->hoverType == HOVER_SOCIAL && g_pState->hoverIndex == (int)i);
        Color color1 = btn.color1;
        Color color2 = btn.color2;
        if (isBtnHovered) {
            color1 = Color(btn.color1.GetA(), (REAL)std::min((int)255, (int)btn.color1.GetR() + 25), (REAL)std::min((int)255, (int)btn.color1.GetG() + 25), (REAL)std::min((int)255, (int)btn.color1.GetB() + 25));
            color2 = Color(btn.color2.GetA(), (REAL)std::min((int)255, (int)btn.color2.GetR() + 25), (REAL)std::min((int)255, (int)btn.color2.GetG() + 25), (REAL)std::min((int)255, (int)btn.color2.GetB() + 25));
        }
        LinearGradientBrush gradBrush(btn.rect, color1, color2, LinearGradientModeVertical);
        FillRoundRect(g, &gradBrush, btn.rect, 10);

        Pen borderPen(Color(100, 255, 255, 255), 1.0f);
        DrawRoundRect(g, &borderPen, btn.rect, 10);

        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        RectF btnRectF((REAL)btn.rect.X, (REAL)btn.rect.Y, (REAL)btn.rect.Width, (REAL)btn.rect.Height);
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        g.DrawString(btn.label.c_str(), -1, &buttonFont, btnRectF, &format, &whiteBrush);
    }

    // Draw Credits Section (scrollable, below social buttons)
    g.DrawString(L"Tasarımcılar ve Yapımcılar", -1, &sectionFont, PointF(30, 965), &textBrush);
    g.DrawString(L"oneko, dog, tora: Tatsuya Kato & Masayuki Koba", -1, &subFont, PointF(30, 990), &subTextBrush);
    g.DrawString(L"sakura, tomoyo: Kiichiroh Mukose", -1, &subFont, PointF(30, 1007), &subTextBrush);
    g.DrawString(L"bsd (BSD Daemon): Marshall Kirk McKusick", -1, &subFont, PointF(30, 1024), &subTextBrush);
    g.DrawString(L"eevee, bunny, black: Onekocord Community / Nintendo", -1, &subFont, PointF(30, 1041), &subTextBrush);
    g.DrawString(L"© 2026 Keday. Tüm hakları saklıdır.", -1, &subFont, PointF(30, 1065), &subTextBrush);

    // Reset translation to draw static bottom buttons and scrollbar
    g.ResetTransform();

    // Draw static footer separator
    g.DrawLine(&dividerPen, 30, WIN_HEIGHT - 60, 420, WIN_HEIGHT - 60);

    // Draw macOS-style scrollbar
    int scrollbarX = WIN_WIDTH - 12;
    int scrollbarYStart = 85;
    int scrollbarHeight = WIN_HEIGHT - 60 - scrollbarYStart - 10;
    
    SolidBrush scrollTrackBrush(dark ? Color(15, 255, 255, 255) : Color(8, 0, 0, 0));
    g.FillRectangle(&scrollTrackBrush, scrollbarX, scrollbarYStart, 6, scrollbarHeight);

    int scrollThumbHeight = 120;
    int scrollThumbMaxY = scrollbarHeight - scrollThumbHeight;
    int scrollThumbY = scrollbarYStart + (int)(((double)g_pState->scrollY / g_pState->maxScrollY) * scrollThumbMaxY);

    Color scrollThumbColor = dark ? Color(120, 255, 255, 255) : Color(80, 0, 0, 0);
    SolidBrush scrollThumbBrush(scrollThumbColor);
    Rect scrollThumbRect(scrollbarX, scrollThumbY, 6, scrollThumbHeight);
    FillRoundRect(g, &scrollThumbBrush, scrollThumbRect, 3);

    // Draw Cancel button
    bool isCancelHovered = (g_pState->hoverType == HOVER_CANCEL_BTN);
    Color cancelCol = isCancelHovered ? (dark ? Color(255, 36, 37, 48) : Color(255, 235, 238, 245)) : cardBgCol;
    SolidBrush cancelBrush(cancelCol);
    FillRoundRect(g, &cancelBrush, g_pState->cancelBtnRect, 10);
    Pen cancelBorder(isCancelHovered ? activeCol1 : borderCol, 1);
    DrawRoundRect(g, &cancelBorder, g_pState->cancelBtnRect, 10);

    StringFormat btnFormat;
    btnFormat.SetAlignment(StringAlignmentCenter);
    btnFormat.SetLineAlignment(StringAlignmentCenter);
    RectF cancelRectF((REAL)g_pState->cancelBtnRect.X, (REAL)g_pState->cancelBtnRect.Y, (REAL)g_pState->cancelBtnRect.Width, (REAL)g_pState->cancelBtnRect.Height);
    g.DrawString(L"İptal Et", -1, &buttonFont, cancelRectF, &btnFormat, &textBrush);

    // Draw OK button
    bool isOkHovered = (g_pState->hoverType == HOVER_OK_BTN);
    Color okCol1 = isOkHovered ? Color(255, 255, 100, 150) : Color(255, 255, 80, 130);
    Color okCol2 = isOkHovered ? Color(255, 230, 40, 90) : Color(255, 200, 30, 80);
    LinearGradientBrush okGrad(g_pState->okBtnRect, okCol1, okCol2, LinearGradientModeVertical);
    FillRoundRect(g, &okGrad, g_pState->okBtnRect, 10);
    Pen okBorder(isOkHovered ? Color(255, 255, 255, 220) : Color(100, 255, 255, 255), 1.0f);
    DrawRoundRect(g, &okBorder, g_pState->okBtnRect, 10);

    RectF okRectF((REAL)g_pState->okBtnRect.X, (REAL)g_pState->okBtnRect.Y, (REAL)g_pState->okBtnRect.Width, (REAL)g_pState->okBtnRect.Height);
    SolidBrush whiteBrush(Color(255, 255, 255, 255));
    g.DrawString(L"Kaydet", -1, &buttonFont, okRectF, &btnFormat, &whiteBrush);

    // BitBlt to screen
    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
}

// Window Procedure for Settings
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_pState = new SettingsWindowState();
            if (g_pSettings) {
                g_pState->originalSettings = *g_pSettings;
            }
            InitializeLayout(hWnd);
            // Setup Timer for character walk animation (150ms loop)
            SetTimer(hWnd, 2, 150, NULL);
            break;
        }

        case WM_TIMER: {
            if (wParam == 2 && g_pState) {
                g_pState->previewFrameIndex++;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_MOUSEWHEEL: {
            if (g_pState) {
                int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
                // Scroll speed: 20 pixels per wheel notch
                g_pState->scrollY -= (zDelta / WHEEL_DELTA) * 20;
                if (g_pState->scrollY < 0) g_pState->scrollY = 0;
                if (g_pState->scrollY > g_pState->maxScrollY) g_pState->scrollY = g_pState->maxScrollY;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintSettings(hWnd, hdc);
            EndPaint(hWnd, &ps);
            break;
        }

        case WM_KEYDOWN: {
            if (g_pState && g_pState->focusedSliderIdx != -1) {
                auto& slider = g_pState->sliders[g_pState->focusedSliderIdx];
                int change = 0;
                if (wParam == VK_LEFT || wParam == VK_DOWN) {
                    change = -1;
                } else if (wParam == VK_RIGHT || wParam == VK_UP) {
                    change = 1;
                }

                if (change != 0) {
                    // Larger jumps for size/opacity/volume
                    if (slider.type != SLIDER_SPEED) change *= 5;

                    *(slider.valPtr) += change;
                    if (*(slider.valPtr) < slider.minVal) *(slider.valPtr) = slider.minVal;
                    if (*(slider.valPtr) > slider.maxVal) *(slider.valPtr) = slider.maxVal;

                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            if (!g_pState) break;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Cancel button hit test (static positioning, not scrolled)
            if (g_pState->cancelBtnRect.Contains(x, y)) {
                if (g_pSettings) {
                    *g_pSettings = g_pState->originalSettings;
                    if (g_onSettingsChanged) g_onSettingsChanged();
                }
                DestroyWindow(hWnd);
                break;
            }

            // OK Button hit test
            if (g_pState->okBtnRect.Contains(x, y)) {
                if (g_pSettings) {
                    SaveSettings(*g_pSettings);
                    SetAutoStart(g_pSettings->autoLaunch);
                }
                DestroyWindow(hWnd);
                break;
            }

            // Convert raw Y to scrolled Y coordinate space for content hits
            int scrolledY = y + g_pState->scrollY;

            // Check Character Cards
            for (const auto& card : g_pState->charCards) {
                if (card.rect.Contains(x, scrolledY)) {
                    if (g_pSettings) {
                        g_pSettings->character = card.name;
                        InvalidateRect(hWnd, NULL, FALSE);
                        if (g_onSettingsChanged) g_onSettingsChanged();
                    }
                    break;
                }
            }

            // Check Sliders
            g_pState->focusedSliderIdx = -1;
            int idx = 0;
            for (auto& slider : g_pState->sliders) {
                Rect hitRect(slider.trackRect.X - 10, slider.trackRect.Y - 10, slider.trackRect.Width + 20, slider.trackRect.Height + 20);
                if (hitRect.Contains(x, scrolledY)) {
                    g_pState->focusedSliderIdx = idx;
                    slider.isDragging = true;
                    SetCapture(hWnd);
                    float pct = (float)(x - slider.trackRect.X) / slider.trackRect.Width;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    *(slider.valPtr) = slider.minVal + (int)(pct * (slider.maxVal - slider.minVal));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
                idx++;
            }

            // Check Toggles (Whole row hit testing)
            for (auto& toggle : g_pState->toggles) {
                if (toggle.rect.Contains(x, scrolledY)) {
                    *(toggle.valPtr) = !(*(toggle.valPtr));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
            }

            // Check Accessory Buttons
            for (const auto& btn : g_pState->accessoryBtns) {
                if (btn.rect.Contains(x, scrolledY)) {
                    if (g_pSettings) {
                        g_pSettings->accessory = btn.value;
                        InvalidateRect(hWnd, NULL, FALSE);
                        if (g_onSettingsChanged) g_onSettingsChanged();
                    }
                    break;
                }
            }

            // Check Social Buttons
            for (const auto& btn : g_pState->socialButtons) {
                if (btn.rect.Contains(x, scrolledY)) {
                    ShellExecuteW(NULL, L"open", btn.url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (!g_pState) break;
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Handle dragging slider
            bool draggingActive = false;
            for (auto& slider : g_pState->sliders) {
                if (slider.isDragging) {
                    draggingActive = true;
                    float pct = (float)(x - slider.trackRect.X) / slider.trackRect.Width;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    *(slider.valPtr) = slider.minVal + (int)(pct * (slider.maxVal - slider.minVal));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
            }

            if (!draggingActive) {
                // Tracking mouse for hover effects
                if (!g_pState->trackingMouse) {
                    TRACKMOUSEEVENT tme = {};
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hWnd;
                    TrackMouseEvent(&tme);
                    g_pState->trackingMouse = true;
                }

                HoverType newHoverType = HOVER_NONE;
                int newHoverIndex = -1;

                // Static footer checks
                if (g_pState->cancelBtnRect.Contains(x, y)) {
                    newHoverType = HOVER_CANCEL_BTN;
                    newHoverIndex = 0;
                } else if (g_pState->okBtnRect.Contains(x, y)) {
                    newHoverType = HOVER_OK_BTN;
                    newHoverIndex = 0;
                } else {
                    // Scrolled layout checks
                    int scrolledY = y + g_pState->scrollY;

                    for (size_t i = 0; i < g_pState->charCards.size(); ++i) {
                        if (g_pState->charCards[i].rect.Contains(x, scrolledY)) {
                            newHoverType = HOVER_CARD;
                            newHoverIndex = (int)i;
                            break;
                        }
                    }

                    if (newHoverType == HOVER_NONE) {
                        for (size_t i = 0; i < g_pState->sliders.size(); ++i) {
                            Rect hitRect(g_pState->sliders[i].trackRect.X - 10, g_pState->sliders[i].trackRect.Y - 10, g_pState->sliders[i].trackRect.Width + 20, g_pState->sliders[i].trackRect.Height + 20);
                            if (hitRect.Contains(x, scrolledY)) {
                                newHoverType = HOVER_SLIDER;
                                newHoverIndex = (int)i;
                                break;
                            }
                        }
                    }

                    if (newHoverType == HOVER_NONE) {
                        for (size_t i = 0; i < g_pState->toggles.size(); ++i) {
                            if (g_pState->toggles[i].rect.Contains(x, scrolledY)) {
                                newHoverType = HOVER_TOGGLE;
                                newHoverIndex = (int)i;
                                break;
                            }
                        }
                    }

                    if (newHoverType == HOVER_NONE) {
                        for (size_t i = 0; i < g_pState->accessoryBtns.size(); ++i) {
                            if (g_pState->accessoryBtns[i].rect.Contains(x, scrolledY)) {
                                newHoverType = HOVER_ACCESSORY;
                                newHoverIndex = (int)i;
                                break;
                            }
                        }
                    }

                    if (newHoverType == HOVER_NONE) {
                        for (size_t i = 0; i < g_pState->socialButtons.size(); ++i) {
                            if (g_pState->socialButtons[i].rect.Contains(x, scrolledY)) {
                                newHoverType = HOVER_SOCIAL;
                                newHoverIndex = (int)i;
                                break;
                            }
                        }
                    }
                }

                if (newHoverType != g_pState->hoverType || newHoverIndex != g_pState->hoverIndex) {
                    g_pState->hoverType = newHoverType;
                    g_pState->hoverIndex = newHoverIndex;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_MOUSELEAVE: {
            if (g_pState) {
                g_pState->trackingMouse = false;
                g_pState->hoverType = HOVER_NONE;
                g_pState->hoverIndex = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (!g_pState) break;
            bool released = false;
            for (auto& slider : g_pState->sliders) {
                if (slider.isDragging) {
                    slider.isDragging = false;
                    released = true;
                }
            }
            if (released) {
                ReleaseCapture();
                if (g_pSettings) {
                    SaveSettings(*g_pSettings);
                    SetAutoStart(g_pSettings->autoLaunch);
                }
            }
            break;
        }

        case WM_CLOSE:
            // Close event: Revert settings unless OK button was explicitly clicked
            // Close event acts as "Cancel" behavior to match expectation of clicking X
            if (g_pState && g_pSettings) {
                *g_pSettings = g_pState->originalSettings;
                if (g_onSettingsChanged) g_onSettingsChanged();
            }
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY: {
            KillTimer(hWnd, 2);
            if (g_pState) {
                for (auto& card : g_pState->charCards) {
                    if (card.previewFrames[0]) delete card.previewFrames[0];
                    if (card.previewFrames[1]) delete card.previewFrames[1];
                }
                delete g_pState;
                g_pState = nullptr;
            }
            g_hSettingsWnd = NULL;
            break;
        }

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void OpenSettingsWindow(HINSTANCE hInstance, HWND hParent, Settings& currentSettings, void (*onSettingsChanged)()) {
    if (g_hSettingsWnd != NULL) {
        SetActiveWindow(g_hSettingsWnd);
        SetForegroundWindow(g_hSettingsWnd);
        return;
    }

    g_pSettings = &currentSettings;
    g_onSettingsChanged = onSettingsChanged;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"KedaySettingsClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    RegisterClassW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - WIN_WIDTH) / 2;
    int y = (screenHeight - WIN_HEIGHT) / 2;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT wr = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
    AdjustWindowRect(&wr, style, FALSE);

    g_hSettingsWnd = CreateWindowExW(
        WS_EX_TOPMOST,
        L"KedaySettingsClass",
        L"Keday Ayarları",
        style,
        x, y,
        wr.right - wr.left,
        wr.bottom - wr.top,
        hParent,
        NULL,
        hInstance,
        NULL
    );

    if (g_hSettingsWnd) {
        ShowWindow(g_hSettingsWnd, SW_SHOW);
        UpdateWindow(g_hSettingsWnd);
    }
}

bool IsSettingsWindowOpen() {
    return g_hSettingsWnd != NULL;
}

void CloseSettingsWindow() {
    if (g_hSettingsWnd != NULL) {
        PostMessage(g_hSettingsWnd, WM_CLOSE, 0, 0);
    }
}
