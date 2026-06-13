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
    SLIDER_OPACITY
};

struct Card {
    Rect rect;
    std::wstring name;
    std::wstring label;
    Bitmap* preview;
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

enum HoverType {
    HOVER_NONE,
    HOVER_CARD,
    HOVER_SLIDER,
    HOVER_TOGGLE,
    HOVER_SOCIAL,
    HOVER_OK_BTN,
    HOVER_CANCEL_BTN
};

struct SettingsWindowState {
    Settings originalSettings;
    std::vector<Card> charCards;
    std::vector<Slider> sliders;
    std::vector<Toggle> toggles;
    std::vector<ButtonControl> socialButtons;
    Rect okBtnRect;
    Rect cancelBtnRect;
    HoverType hoverType = HOVER_NONE;
    int hoverIndex = -1;
    bool trackingMouse = false;
    int previewFrameIndex = 0;
    int scrollY = 0;
    int maxScrollY = 120;
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

    // 1. Character Cards
    std::wstring chars[] = { L"oneko", L"red", L"green", L"rw", L"anime" };
    std::wstring charLabels[] = { L"Keday", L"Kırmızı", L"Yeşil", L"Siyah-Beyaz", L"Anime Kızı" };
    for (int i = 0; i < 5; ++i) {
        Card card;
        card.rect = Rect(20 + i * 82, 90, 72, 72);
        card.name = chars[i];
        card.label = charLabels[i];
        std::wstring baseDir = GetExeDir();
        std::wstring path = baseDir + L"\\assets\\" + chars[i] + L".gif";
        card.preview = Bitmap::FromFile(path.c_str());
        if (!card.preview || card.preview->GetLastStatus() != Ok) {
            if (card.preview) delete card.preview;
            path = baseDir + L"\\assets\\" + chars[i] + L".png";
            card.preview = Bitmap::FromFile(path.c_str());
        }
        g_pState->charCards.push_back(card);
    }

    // 2. Sliders
    if (g_pSettings) {
        Slider speed;
        speed.trackRect = Rect(30, 220, 390, 8);
        speed.valPtr = &g_pSettings->speed;
        speed.minVal = 1;
        speed.maxVal = 30;
        speed.label = L"Yürüme Hızı";
        speed.type = SLIDER_SPEED;
        speed.isDragging = false;
        g_pState->sliders.push_back(speed);

        Slider size;
        size.trackRect = Rect(30, 275, 390, 8);
        size.valPtr = &g_pSettings->size;
        size.minVal = 50;
        size.maxVal = 300;
        size.label = L"Karakter Boyutu";
        size.type = SLIDER_SIZE;
        size.isDragging = false;
        g_pState->sliders.push_back(size);

        Slider opacity;
        opacity.trackRect = Rect(30, 330, 390, 8);
        opacity.valPtr = &g_pSettings->opacity;
        opacity.minVal = 10;
        opacity.maxVal = 100;
        opacity.label = L"Şeffaflık";
        opacity.type = SLIDER_OPACITY;
        opacity.isDragging = false;
        g_pState->sliders.push_back(opacity);

        // 3. Toggles (2 columns, Y-spacing adjusted)
        Toggle t1 = { Rect(30, 370, 180, 30), &g_pSettings->alwaysOnTop, L"Her Zaman Üstte" };
        Toggle t2 = { Rect(30, 410, 180, 30), &g_pSettings->followMouse, L"Fareyi Takip Et" };
        Toggle t3 = { Rect(30, 450, 180, 30), &g_pSettings->showShadow, L"Gölge Göster" };
        Toggle t4 = { Rect(30, 490, 180, 30), &g_pSettings->isDarkMode, L"Koyu Tema" };
        
        Toggle t5 = { Rect(240, 370, 180, 30), &g_pSettings->minimizeToTray, L"Tepsiye Küçült" };
        Toggle t6 = { Rect(240, 410, 180, 30), &g_pSettings->autoLaunch, L"Başlangıçta Aç" };
        Toggle t7 = { Rect(240, 450, 180, 30), &g_pSettings->hideOnFullscreen, L"Tam Ekran Gizle" };

        g_pState->toggles.push_back(t1);
        g_pState->toggles.push_back(t2);
        g_pState->toggles.push_back(t3);
        g_pState->toggles.push_back(t4);
        g_pState->toggles.push_back(t5);
        g_pState->toggles.push_back(t6);
        g_pState->toggles.push_back(t7);
    }

    // 4. Social Buttons
    ButtonControl discord = { Rect(30, 550, 115, 36), L"Discord", L"https://discord.gg/hentaitr", Color(255, 114, 137, 218), Color(255, 91, 110, 174) };
    ButtonControl github = { Rect(167, 550, 115, 36), L"GitHub", L"https://github.com/blackeker/Keday", Color(255, 51, 51, 51), Color(255, 36, 41, 46) };
    ButtonControl insta = { Rect(305, 550, 115, 36), L"Instagram", L"https://instagram.com/blackekerr", Color(255, 240, 148, 51), Color(255, 188, 24, 136) };
    g_pState->socialButtons.push_back(discord);
    g_pState->socialButtons.push_back(github);
    g_pState->socialButtons.push_back(insta);

    // 5. Buttons (OK & Cancel side by side)
    g_pState->cancelBtnRect = Rect(30, 610, 180, 40);
    g_pState->okBtnRect = Rect(240, 610, 180, 40);
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

    // Theme Color Selection
    bool dark = g_pSettings->isDarkMode;
    Color bgCol = dark ? Color(255, 26, 26, 30) : Color(255, 242, 244, 248);
    Color textCol = dark ? Color(255, 240, 240, 245) : Color(255, 25, 27, 38);
    Color subTextCol = dark ? Color(255, 166, 173, 200) : Color(255, 110, 115, 130);
    Color cardBgCol = dark ? Color(255, 40, 40, 45) : Color(255, 255, 255, 255);
    Color borderCol = dark ? Color(255, 55, 55, 60) : Color(255, 218, 222, 230);
    Color activeCol = dark ? Color(255, 255, 111, 145) : Color(255, 255, 75, 110);

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
    SolidBrush activeBrush(activeCol);

    // Push translation for scrolling (affects characters, sliders, toggles, socials)
    // Keep header static at the top
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
        Color currentCardBg = isHovered ? (dark ? Color(255, 60, 62, 95) : Color(255, 242, 244, 255)) : cardBgCol;
        SolidBrush cardBrush(currentCardBg);
        FillRoundRect(g, &cardBrush, card.rect, 10);

        if (isSelected) {
            Pen activePen(activeCol, 2);
            DrawRoundRect(g, &activePen, card.rect, 10);
        } else if (isHovered) {
            Pen hoverPen(dark ? Color(255, 255, 130, 160) : Color(255, 255, 110, 140), 1.5f);
            DrawRoundRect(g, &hoverPen, card.rect, 10);
        } else {
            Pen borderPen(borderCol, 1);
            DrawRoundRect(g, &borderPen, card.rect, 10);
        }

        // Draw animated character preview
        if (card.preview && card.preview->GetLastStatus() == Ok) {
            int pW = card.preview->GetWidth() / 8;
            int pH = (card.name == L"anime") ? (card.preview->GetHeight() / 8) : (card.preview->GetHeight() / 4);
            
            int animCol = 0;
            int animRow = 0;
            if (card.name == L"anime") {
                animCol = g_pState->previewFrameIndex % 4;
                animRow = 1; // Walk East
            } else {
                // Loop east running animation frames (cols 3 & 4 on row 0)
                animCol = 3 + (g_pState->previewFrameIndex % 2);
                animRow = 0;
            }
            g.DrawImage(card.preview, Rect(card.rect.X + (card.rect.Width - 40)/2, card.rect.Y + 6, 40, 40), animCol*pW, animRow*pH, pW, pH, UnitPixel);
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
        SolidBrush trackBrush(cardBgCol);
        FillRoundRect(g, &trackBrush, slider.trackRect, 4);

        // Active part
        int currentVal = *(slider.valPtr);
        float percent = (float)(currentVal - slider.minVal) / (slider.maxVal - slider.minVal);
        int activeWidth = (int)(slider.trackRect.Width * percent);
        Rect activeRect(slider.trackRect.X, slider.trackRect.Y, activeWidth, slider.trackRect.Height);
        FillRoundRect(g, &activeBrush, activeRect, 4);

        // Focused slider highlight
        if (g_pState->focusedSliderIdx == (int)i) {
            Pen focusPen(activeCol, 1.2f);
            focusPen.SetDashStyle(DashStyleDash);
            Rect focusRect(slider.trackRect.X - 4, slider.trackRect.Y - 4, slider.trackRect.Width + 8, slider.trackRect.Height + 8);
            g.DrawRectangle(&focusPen, focusRect);
        }

        // Thumb
        int thumbX = slider.trackRect.X + activeWidth;
        int thumbY = slider.trackRect.Y + slider.trackRect.Height / 2;
        bool isSliderHovered = (g_pState->hoverType == HOVER_SLIDER && g_pState->hoverIndex == (int)i);
        int thumbRadius = isSliderHovered ? 9 : 7;
        SolidBrush thumbBrush(Color(255, 255, 255, 255));
        g.FillEllipse(&thumbBrush, thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        Pen thumbBorderPen(isSliderHovered ? (dark ? Color(255, 255, 130, 160) : Color(255, 255, 110, 140)) : activeCol, 1.5f);
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
        Color toggleBgColor = state ? (isToggleHovered ? (dark ? Color(255, 255, 130, 160) : Color(255, 255, 110, 140)) : activeCol) : (isToggleHovered ? (dark ? Color(255, 60, 62, 95) : Color(255, 230, 235, 240)) : cardBgCol);
        SolidBrush switchBg(toggleBgColor);
        FillRoundRect(g, &switchBg, switchRect, 10);
        Pen switchBorder(isToggleHovered ? (dark ? Color(255, 255, 130, 160) : Color(255, 255, 110, 140)) : borderCol, 1);
        if (!state) DrawRoundRect(g, &switchBorder, switchRect, 10);

        // Switch knob
        SolidBrush knobBrush(Color(255, 255, 255, 255));
        int knobX = state ? (switchRect.X + 22) : (switchRect.X + 2);
        g.FillEllipse(&knobBrush, knobX, switchRect.Y + 2, 16, 16);

        // Label
        g.DrawString(toggle.label.c_str(), -1, &labelFont, PointF((REAL)toggle.rect.X, (REAL)toggle.rect.Y + 5), &textBrush);
        
        // Hover border around the entire toggle row hit area if hovered
        if (isToggleHovered) {
            Pen rowHoverPen(dark ? Color(60, 255, 255, 255) : Color(30, 0, 0, 0), 1.0f);
            g.DrawRectangle(&rowHoverPen, toggle.rect);
        }
    }

    // Draw Social Buttons
    for (size_t i = 0; i < g_pState->socialButtons.size(); ++i) {
        const auto& btn = g_pState->socialButtons[i];
        bool isBtnHovered = (g_pState->hoverType == HOVER_SOCIAL && g_pState->hoverIndex == (int)i);
        Color color1 = btn.color1;
        Color color2 = btn.color2;
        if (isBtnHovered) {
            color1 = Color(btn.color1.GetA(), (REAL)std::min((int)255, (int)btn.color1.GetR() + 30), (REAL)std::min((int)255, (int)btn.color1.GetG() + 30), (REAL)std::min((int)255, (int)btn.color1.GetB() + 30));
            color2 = Color(btn.color2.GetA(), (REAL)std::min((int)255, (int)btn.color2.GetR() + 30), (REAL)std::min((int)255, (int)btn.color2.GetG() + 30), (REAL)std::min((int)255, (int)btn.color2.GetB() + 30));
        }
        LinearGradientBrush gradBrush(btn.rect, color1, color2, LinearGradientModeVertical);
        FillRoundRect(g, &gradBrush, btn.rect, 8);

        if (isBtnHovered) {
            Pen hoverPen(Color(255, 255, 255, 200), 1.5f);
            DrawRoundRect(g, &hoverPen, btn.rect, 8);
        }

        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        RectF btnRectF((REAL)btn.rect.X, (REAL)btn.rect.Y, (REAL)btn.rect.Width, (REAL)btn.rect.Height);
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        g.DrawString(btn.label.c_str(), -1, &buttonFont, btnRectF, &format, &whiteBrush);
    }

    // Reset translation to draw static bottom buttons
    g.ResetTransform();

    // Draw static footer separator
    g.DrawLine(&dividerPen, 30, WIN_HEIGHT - 60, 420, WIN_HEIGHT - 60);

    // Draw Cancel button
    bool isCancelHovered = (g_pState->hoverType == HOVER_CANCEL_BTN);
    Color cancelCol = isCancelHovered ? (dark ? Color(255, 55, 55, 60) : Color(255, 220, 225, 235)) : cardBgCol;
    SolidBrush cancelBrush(cancelCol);
    FillRoundRect(g, &cancelBrush, g_pState->cancelBtnRect, 10);
    Pen cancelBorder(isCancelHovered ? activeCol : borderCol, 1);
    DrawRoundRect(g, &cancelBorder, g_pState->cancelBtnRect, 10);

    StringFormat btnFormat;
    btnFormat.SetAlignment(StringAlignmentCenter);
    btnFormat.SetLineAlignment(StringAlignmentCenter);
    RectF cancelRectF((REAL)g_pState->cancelBtnRect.X, (REAL)g_pState->cancelBtnRect.Y, (REAL)g_pState->cancelBtnRect.Width, (REAL)g_pState->cancelBtnRect.Height);
    g.DrawString(L"İptal Et", -1, &buttonFont, cancelRectF, &btnFormat, &textBrush);

    // Draw OK button
    bool isOkHovered = (g_pState->hoverType == HOVER_OK_BTN);
    Color okCol1 = isOkHovered ? Color(255, 128, 119, 255) : Color(255, 108, 99, 255);
    Color okCol2 = isOkHovered ? Color(255, 94, 89, 197) : Color(255, 74, 69, 177);
    LinearGradientBrush okGrad(g_pState->okBtnRect, okCol1, okCol2, LinearGradientModeVertical);
    FillRoundRect(g, &okGrad, g_pState->okBtnRect, 10);
    Pen okBorder(isOkHovered ? Color(255, 255, 255, 220) : borderCol, 1);
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
                    // Larger jumps for size/opacity
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
                    if (card.preview) delete card.preview;
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
