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

// Layout definitions
const int WIN_WIDTH = 450;
const int WIN_HEIGHT = 650;

// Element rects
struct Card {
    Rect rect;
    std::wstring name;
    std::wstring label;
    Bitmap* preview;
};
static std::vector<Card> g_charCards;

struct Slider {
    Rect trackRect;
    int* valPtr;
    int minVal;
    int maxVal;
    std::wstring label;
    bool isDragging;
};
static std::vector<Slider> g_sliders;

struct Toggle {
    Rect rect;
    bool* valPtr;
    std::wstring label;
};
static std::vector<Toggle> g_toggles;

struct ButtonControl {
    Rect rect;
    std::wstring label;
    std::wstring url;
    Color color1;
    Color color2;
};
static std::vector<ButtonControl> g_socialButtons;
static Rect g_okBtnRect;

enum HoverType {
    HOVER_NONE,
    HOVER_CARD,
    HOVER_SLIDER,
    HOVER_TOGGLE,
    HOVER_SOCIAL,
    HOVER_OK_BTN
};
static HoverType g_hoverType = HOVER_NONE;
static int g_hoverIndex = -1;
static bool g_trackingMouse = false;

static void InitializeLayout(HWND hWnd) {
    g_charCards.clear();
    g_sliders.clear();
    g_toggles.clear();
    g_socialButtons.clear();

    // 1. Character Cards
    std::wstring chars[] = { L"oneko", L"red", L"green", L"rw" };
    std::wstring charLabels[] = { L"Keday", L"Kırmızı", L"Yeşil", L"Siyah-Beyaz" };
    for (int i = 0; i < 4; ++i) {
        Card card;
        card.rect = Rect(30 + i * 98, 90, 84, 84);
        card.name = chars[i];
        card.label = charLabels[i];
        // Load first frame for preview (col 3, row 3 in 8x4 sprite sheet)
        std::wstring baseDir = GetExeDir();
        std::wstring path = baseDir + L"\\assets\\" + chars[i] + L".gif";
        card.preview = Bitmap::FromFile(path.c_str());
        if (!card.preview || card.preview->GetLastStatus() != Ok) {
            if (card.preview) delete card.preview;
            path = baseDir + L"\\assets\\" + chars[i] + L".png";
            card.preview = Bitmap::FromFile(path.c_str());
        }
        g_charCards.push_back(card);
    }

    // 2. Sliders
    if (g_pSettings) {
        Slider speed;
        speed.trackRect = Rect(30, 220, 390, 8);
        speed.valPtr = &g_pSettings->speed;
        speed.minVal = 1;
        speed.maxVal = 30;
        speed.label = L"Yürüme Hızı";
        speed.isDragging = false;
        g_sliders.push_back(speed);

        Slider size;
        size.trackRect = Rect(30, 275, 390, 8);
        size.valPtr = &g_pSettings->size;
        size.minVal = 50;
        size.maxVal = 300;
        size.label = L"Karakter Boyutu";
        size.isDragging = false;
        g_sliders.push_back(size);

        Slider opacity;
        opacity.trackRect = Rect(30, 330, 390, 8);
        opacity.valPtr = &g_pSettings->opacity;
        opacity.minVal = 10;
        opacity.maxVal = 100;
        opacity.label = L"Şeffaflık";
        opacity.isDragging = false;
        g_sliders.push_back(opacity);

        // 3. Toggles (2 columns)
        Toggle t1 = { Rect(30, 370, 180, 30), &g_pSettings->alwaysOnTop, L"Her Zaman Üstte" };
        Toggle t2 = { Rect(30, 410, 180, 30), &g_pSettings->followMouse, L"Fareyi Takip Et" };
        Toggle t3 = { Rect(30, 450, 180, 30), &g_pSettings->showShadow, L"Gölge Göster" };
        
        Toggle t4 = { Rect(240, 370, 180, 30), &g_pSettings->minimizeToTray, L"Tepsiye Küçült" };
        Toggle t5 = { Rect(240, 410, 180, 30), &g_pSettings->autoLaunch, L"Başlangıçta Aç" };
        Toggle t6 = { Rect(240, 450, 180, 30), &g_pSettings->hideOnFullscreen, L"Tam Ekran Gizle" };

        g_toggles.push_back(t1);
        g_toggles.push_back(t2);
        g_toggles.push_back(t3);
        g_toggles.push_back(t4);
        g_toggles.push_back(t5);
        g_toggles.push_back(t6);
    }

    // 4. Social Buttons
    ButtonControl discord = { Rect(30, 510, 115, 36), L"Discord", L"https://discord.gg/hentaitr", Color(255, 114, 137, 218), Color(255, 91, 110, 174) };
    ButtonControl github = { Rect(167, 510, 115, 36), L"GitHub", L"https://github.com/blackeker/Keday", Color(255, 51, 51, 51), Color(255, 36, 41, 46) };
    ButtonControl insta = { Rect(305, 510, 115, 36), L"Instagram", L"https://instagram.com/blackekerr", Color(255, 240, 148, 51), Color(255, 188, 24, 136) };
    g_socialButtons.push_back(discord);
    g_socialButtons.push_back(github);
    g_socialButtons.push_back(insta);

    // 5. OK button
    g_okBtnRect = Rect(150, 565, 150, 40);
}

static void PaintSettings(HWND hWnd, HDC hdc) {
    RECT rect;
    GetClientRect(hWnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Double buffering
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    Graphics g(hMemDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    // Color Palette
    Color bgCol(255, 30, 30, 46); // Dark blue-grey
    Color cardBgCol(255, 45, 47, 72);
    Color borderCol(255, 88, 91, 112);
    Color activeCol(255, 108, 99, 255); // Purple
    Color textCol(255, 205, 214, 244);
    Color subTextCol(255, 166, 173, 200);

    SolidBrush bgBrush(bgCol);
    g.FillRectangle(&bgBrush, 0, 0, width, height);

    // Fonts
    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 20, FontStyleBold, UnitPoint);
    Font subFont(&fontFamily, 9, FontStyleRegular, UnitPoint);
    Font sectionFont(&fontFamily, 11, FontStyleBold, UnitPoint);
    Font labelFont(&fontFamily, 10, FontStyleRegular, UnitPoint);
    Font valueFont(&fontFamily, 9, FontStyleBold, UnitPoint);
    Font buttonFont(&fontFamily, 10, FontStyleBold, UnitPoint);

    SolidBrush textBrush(textCol);
    SolidBrush subTextBrush(subTextCol);
    SolidBrush activeBrush(activeCol);

    // Draw Title
    g.DrawString(L"Keday Ayarları", -1, &titleFont, PointF(30, 20), &textBrush);
    g.DrawString(L"Masaüstü kedisi yapılandırma paneli", -1, &subFont, PointF(30, 52), &subTextBrush);

    // Divider
    Pen dividerPen(borderCol, 1);
    g.DrawLine(&dividerPen, 30, 75, 420, 75);

    // Draw Character Cards
    for (int i = 0; i < g_charCards.size(); ++i) {
        const auto& card = g_charCards[i];
        bool isSelected = (g_pSettings && g_pSettings->character == card.name);
        bool isHovered = (g_hoverType == HOVER_CARD && g_hoverIndex == i);
        Color currentCardBg = isHovered ? Color(255, 60, 62, 95) : cardBgCol;
        SolidBrush cardBrush(currentCardBg);
        FillRoundRect(g, &cardBrush, card.rect, 10);

        if (isSelected) {
            Pen activePen(activeCol, 2);
            DrawRoundRect(g, &activePen, card.rect, 10);
        } else if (isHovered) {
            Pen hoverPen(Color(255, 137, 131, 255), 1.5f);
            DrawRoundRect(g, &hoverPen, card.rect, 10);
        } else {
            Pen borderPen(borderCol, 1);
            DrawRoundRect(g, &borderPen, card.rect, 10);
        }

        // Draw preview image
        if (card.preview && card.preview->GetLastStatus() == Ok) {
            // Frame is at (3, 3) in the 8x4 sheet
            int pW = card.preview->GetWidth() / 8;
            int pH = card.preview->GetHeight() / 4;
            g.DrawImage(card.preview, Rect(card.rect.X + (card.rect.Width - 48)/2, card.rect.Y + 10, 48, 48), 3*pW, 3*pH, pW, pH, UnitPixel);
        }

        // Draw label
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        RectF textRect((REAL)card.rect.X, (REAL)card.rect.Y + 62, (REAL)card.rect.Width, 20.0f);
        g.DrawString(card.label.c_str(), -1, &subFont, textRect, &format, &textBrush);
    }

    // Draw Sliders
    int sliderIdx = 0;
    for (const auto& slider : g_sliders) {
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

        // Thumb
        int thumbX = slider.trackRect.X + activeWidth;
        int thumbY = slider.trackRect.Y + slider.trackRect.Height / 2;
        bool isSliderHovered = (g_hoverType == HOVER_SLIDER && g_hoverIndex == sliderIdx);
        int thumbRadius = isSliderHovered ? 10 : 8;
        SolidBrush thumbBrush(Color(255, 255, 255, 255));
        g.FillEllipse(&thumbBrush, thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);
        Pen thumbBorderPen(isSliderHovered ? Color(255, 137, 131, 255) : activeCol, 1.5f);
        g.DrawEllipse(&thumbBorderPen, thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2, thumbRadius * 2);

        // Value display text
        std::wstring valText;
        if (slider.label == L"Şeffaflık") valText = L"%" + std::to_wstring(currentVal);
        else if (slider.label == L"Karakter Boyutu") valText = L"%" + std::to_wstring(currentVal);
        else valText = std::to_wstring(currentVal);

        StringFormat valFormat;
        valFormat.SetAlignment(StringAlignmentFar);
        RectF valRect((REAL)slider.trackRect.X, (REAL)slider.trackRect.Y - 22, (REAL)slider.trackRect.Width, 20.0f);
        g.DrawString(valText.c_str(), -1, &valueFont, valRect, &valFormat, &activeBrush);
        
        sliderIdx++;
    }

    // Draw Toggles
    int toggleIdx = 0;
    for (const auto& toggle : g_toggles) {
        bool state = *(toggle.valPtr);
        bool isToggleHovered = (g_hoverType == HOVER_TOGGLE && g_hoverIndex == toggleIdx);

        // Switch box (size 40x20 at right of toggle rect)
        Rect switchRect(toggle.rect.X + toggle.rect.Width - 40, toggle.rect.Y + 5, 40, 20);
        Color toggleBgColor = state ? (isToggleHovered ? Color(255, 137, 131, 255) : activeCol) : (isToggleHovered ? Color(255, 60, 62, 95) : cardBgCol);
        SolidBrush switchBg(toggleBgColor);
        FillRoundRect(g, &switchBg, switchRect, 10);
        Pen switchBorder(isToggleHovered ? Color(255, 137, 131, 255) : borderCol, 1);
        if (!state) DrawRoundRect(g, &switchBorder, switchRect, 10);

        // Switch knob
        SolidBrush knobBrush(Color(255, 255, 255, 255));
        int knobX = state ? (switchRect.X + 22) : (switchRect.X + 2);
        g.FillEllipse(&knobBrush, knobX, switchRect.Y + 2, 16, 16);

        // Label
        g.DrawString(toggle.label.c_str(), -1, &labelFont, PointF((REAL)toggle.rect.X, (REAL)toggle.rect.Y + 5), &textBrush);
        
        toggleIdx++;
    }

    // Draw Social Buttons
    int btnIdx = 0;
    for (const auto& btn : g_socialButtons) {
        bool isBtnHovered = (g_hoverType == HOVER_SOCIAL && g_hoverIndex == btnIdx);
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
        
        btnIdx++;
    }

    // Draw OK button
    bool isOkHovered = (g_hoverType == HOVER_OK_BTN);
    Color okCol1 = isOkHovered ? Color(255, 128, 119, 255) : Color(255, 108, 99, 255);
    Color okCol2 = isOkHovered ? Color(255, 94, 89, 197) : Color(255, 74, 69, 177);
    LinearGradientBrush okGrad(g_okBtnRect, okCol1, okCol2, LinearGradientModeVertical);
    FillRoundRect(g, &okGrad, g_okBtnRect, 10);
    Pen okBorder(isOkHovered ? Color(255, 255, 255, 220) : borderCol, 1);
    DrawRoundRect(g, &okBorder, g_okBtnRect, 10);

    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    RectF okRectF((REAL)g_okBtnRect.X, (REAL)g_okBtnRect.Y, (REAL)g_okBtnRect.Width, (REAL)g_okBtnRect.Height);
    SolidBrush whiteBrush(Color(255, 255, 255, 255));
    g.DrawString(L"Değişiklikleri Kaydet", -1, &buttonFont, okRectF, &format, &whiteBrush);

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
        case WM_CREATE:
            InitializeLayout(hWnd);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            PaintSettings(hWnd, hdc);
            EndPaint(hWnd, &ps);
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Check Character Cards
            for (const auto& card : g_charCards) {
                if (card.rect.Contains(x, y)) {
                    if (g_pSettings) {
                        g_pSettings->character = card.name;
                        InvalidateRect(hWnd, NULL, FALSE);
                        if (g_onSettingsChanged) g_onSettingsChanged();
                    }
                    break;
                }
            }

            // Check Sliders
            for (auto& slider : g_sliders) {
                // Expand track hit area slightly for better usability
                Rect hitRect(slider.trackRect.X - 10, slider.trackRect.Y - 10, slider.trackRect.Width + 20, slider.trackRect.Height + 20);
                if (hitRect.Contains(x, y)) {
                    slider.isDragging = true;
                    SetCapture(hWnd);
                    // Update value
                    float pct = (float)(x - slider.trackRect.X) / slider.trackRect.Width;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    *(slider.valPtr) = slider.minVal + (int)(pct * (slider.maxVal - slider.minVal));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
            }

            // Check Toggles
            for (auto& toggle : g_toggles) {
                if (toggle.rect.Contains(x, y)) {
                    *(toggle.valPtr) = !(*(toggle.valPtr));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
            }

            // Check Social Buttons
            for (const auto& btn : g_socialButtons) {
                if (btn.rect.Contains(x, y)) {
                    ShellExecuteW(NULL, L"open", btn.url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    break;
                }
            }

            // Check OK Button
            if (g_okBtnRect.Contains(x, y)) {
                if (g_pSettings) {
                    SaveSettings(*g_pSettings);
                    SetAutoStart(g_pSettings->autoLaunch);
                }
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT && g_hoverType != HOVER_NONE) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
            return DefWindowProc(hWnd, message, wParam, lParam);

        case WM_MOUSELEAVE:
            g_trackingMouse = false;
            if (g_hoverType != HOVER_NONE || g_hoverIndex != -1) {
                g_hoverType = HOVER_NONE;
                g_hoverIndex = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Register Mouse Leave Tracking
            if (!g_trackingMouse) {
                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
                g_trackingMouse = true;
            }

            // Handle slider dragging
            bool draggingAny = false;
            for (auto& slider : g_sliders) {
                if (slider.isDragging) {
                    draggingAny = true;
                    float pct = (float)(x - slider.trackRect.X) / slider.trackRect.Width;
                    if (pct < 0.0f) pct = 0.0f;
                    if (pct > 1.0f) pct = 1.0f;
                    *(slider.valPtr) = slider.minVal + (int)(pct * (slider.maxVal - slider.minVal));
                    InvalidateRect(hWnd, NULL, FALSE);
                    if (g_onSettingsChanged) g_onSettingsChanged();
                    break;
                }
            }

            // Handle hover states if not dragging
            if (!draggingAny) {
                HoverType newHoverType = HOVER_NONE;
                int newHoverIndex = -1;

                // Check Cards
                for (int i = 0; i < g_charCards.size(); ++i) {
                    if (g_charCards[i].rect.Contains(x, y)) {
                        newHoverType = HOVER_CARD;
                        newHoverIndex = i;
                        break;
                    }
                }

                // Check Sliders
                if (newHoverType == HOVER_NONE) {
                    for (int i = 0; i < g_sliders.size(); ++i) {
                        Rect hitRect(g_sliders[i].trackRect.X - 10, g_sliders[i].trackRect.Y - 10, g_sliders[i].trackRect.Width + 20, g_sliders[i].trackRect.Height + 20);
                        if (hitRect.Contains(x, y)) {
                            newHoverType = HOVER_SLIDER;
                            newHoverIndex = i;
                            break;
                        }
                    }
                }

                // Check Toggles
                if (newHoverType == HOVER_NONE) {
                    for (int i = 0; i < g_toggles.size(); ++i) {
                        if (g_toggles[i].rect.Contains(x, y)) {
                            newHoverType = HOVER_TOGGLE;
                            newHoverIndex = i;
                            break;
                        }
                    }
                }

                // Check Social Buttons
                if (newHoverType == HOVER_NONE) {
                    for (int i = 0; i < g_socialButtons.size(); ++i) {
                        if (g_socialButtons[i].rect.Contains(x, y)) {
                            newHoverType = HOVER_SOCIAL;
                            newHoverIndex = i;
                            break;
                        }
                    }
                }

                // Check OK Button
                if (newHoverType == HOVER_NONE) {
                    if (g_okBtnRect.Contains(x, y)) {
                        newHoverType = HOVER_OK_BTN;
                        newHoverIndex = 0;
                    }
                }

                // Repaint if hover state changed
                if (newHoverType != g_hoverType || newHoverIndex != g_hoverIndex) {
                    g_hoverType = newHoverType;
                    g_hoverIndex = newHoverIndex;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            break;
        }

        case WM_LBUTTONUP: {
            bool released = false;
            for (auto& slider : g_sliders) {
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
            if (g_pSettings) {
                SaveSettings(*g_pSettings);
                SetAutoStart(g_pSettings->autoLaunch);
            }
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            for (auto& card : g_charCards) {
                if (card.preview) delete card.preview;
            }
            g_hSettingsWnd = NULL;
            break;

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

    // Calculate window rect centered on parent/screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - WIN_WIDTH) / 2;
    int y = (screenHeight - WIN_HEIGHT) / 2;

    // Window style: No maximize/resize to keep it looking clean
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT wr = { 0, 0, WIN_WIDTH, WIN_HEIGHT };
    AdjustWindowRect(&wr, style, FALSE);

    g_hSettingsWnd = CreateWindowExW(
        WS_EX_TOPMOST, // Topmost window
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
