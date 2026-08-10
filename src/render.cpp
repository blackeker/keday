// render.cpp – All GDI+ rendering for Keday
#include "render.h"
#include "features.h"
#include <ctime>
#include <cmath>
#include <algorithm>

// ── Monitor bounds ────────────────────────────────────────────
void GetNekoMonitorBounds(int x, int y, RECT& rect) {
    POINT pt = { x, y };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(MONITORINFO) };
    if (GetMonitorInfoW(hMon, &mi)) {
        rect = mi.rcWork;
    } else {
        rect.left   = 0;
        rect.top    = 0;
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
}

// ── Fullscreen detection ──────────────────────────────────────
bool IsFullscreenWindowActive() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    wchar_t className[256];
    GetClassNameW(hwnd, className, 256);
    std::wstring cls(className);
    if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd") return false;

    RECT rc;
    if (GetWindowRect(hwnd, &rc)) {
        if ((rc.right - rc.left) >= GetSystemMetrics(SM_CXSCREEN) &&
            (rc.bottom - rc.top) >= GetSystemMetrics(SM_CYSCREEN)) {
            if (!(GetWindowLongW(hwnd, GWL_STYLE) & WS_CAPTION)) return true;
        }
    }
    return false;
}

// ── Particle spawner ──────────────────────────────────────────
void SpawnHearts(double startX, double startY, int count, wchar_t symbol) {
    for (int i = 0; i < count; ++i) {
        HeartParticle p;
        p.x      = startX + (rand() % 24 - 12);
        p.y      = startY + (rand() % 10 - 5);
        p.vx     = (rand() % 100 - 50) / 100.0;
        p.vy     = -(rand() % 100 + 50) / 50.0;
        p.life   = 12 + rand() % 8;
        p.symbol = symbol;
        g_particles.push_back(p);
    }
}

// ── Accessory drawing ─────────────────────────────────────────
void DrawAccessory(Graphics& g, int x, int y, int size, int type) {
    if (type <= 0) return;

    int offsetX = 0, offsetY = 0;
    // Horizontal shift by walking direction
    if (g_currentState == STATE_W || g_currentState == STATE_NW || g_currentState == STATE_SW)
        offsetX = -(int)(size * 0.10);
    else if (g_currentState == STATE_E || g_currentState == STATE_NE || g_currentState == STATE_SE)
        offsetX =  (int)(size * 0.10);
    // Vertical shift by state
    if      (g_currentState == STATE_SLEEPING) offsetY =  (int)(size * 0.16);
    else if (g_currentState == STATE_TIRED)    offsetY =  (int)(size * 0.08);
    else if (g_currentState == STATE_N || g_currentState == STATE_NE || g_currentState == STATE_NW)
        offsetY = -(int)(size * 0.05);
    else if (g_currentState == STATE_S || g_currentState == STATE_SE || g_currentState == STATE_SW)
        offsetY =  (int)(size * 0.05);

    x += offsetX;
    y += offsetY;

    if (type == 1) {
        // Glasses
        Pen pen(Color(255, 0, 0, 0), (REAL)(size / 20.0f));
        g.DrawRectangle(&pen, x + (int)(size*.32), y + (int)(size*.38), (int)(size*.16), (int)(size*.12));
        g.DrawRectangle(&pen, x + (int)(size*.52), y + (int)(size*.38), (int)(size*.16), (int)(size*.12));
        g.DrawLine(&pen, x + (int)(size*.48), y + (int)(size*.44),
                         x + (int)(size*.52), y + (int)(size*.44));
    }
    else if (type == 2) {
        // Santa hat
        SolidBrush red(Color(255, 230, 40, 40)), white(Color(255, 245, 245, 245));
        Point pts[] = {
            Point(x + (int)(size*.35), y + (int)(size*.22)),
            Point(x + (int)(size*.65), y + (int)(size*.22)),
            Point(x + (int)(size*.50), y + (int)(size*.04))
        };
        g.FillPolygon(&red, pts, 3);
        g.FillRectangle(&white, x + (int)(size*.30), y + (int)(size*.20), (int)(size*.40), (int)(size*.06));
        g.FillEllipse(&white,   x + (int)(size*.46), y + (int)(size*.01), (int)(size*.08), (int)(size*.08));
    }
    else if (type == 3) {
        // Bow tie
        SolidBrush red(Color(255, 220, 20, 60)), center(Color(255, 150, 10, 40));
        Point left[]  = { Point(x+(int)(size*.38), y+(int)(size*.56)),
                          Point(x+(int)(size*.38), y+(int)(size*.68)),
                          Point(x+(int)(size*.50), y+(int)(size*.62)) };
        Point right[] = { Point(x+(int)(size*.62), y+(int)(size*.56)),
                          Point(x+(int)(size*.62), y+(int)(size*.68)),
                          Point(x+(int)(size*.50), y+(int)(size*.62)) };
        g.FillPolygon(&red, left,  3);
        g.FillPolygon(&red, right, 3);
        g.FillEllipse(&center, x+(int)(size*.47), y+(int)(size*.59), (int)(size*.06), (int)(size*.06));
    }
    else if (type == 4) {
        // Crown
        SolidBrush gold(Color(255, 255, 200, 0));
        Point crown[] = {
            Point(x+(int)(size*.30), y+(int)(size*.20)),
            Point(x+(int)(size*.35), y+(int)(size*.10)),
            Point(x+(int)(size*.50), y+(int)(size*.18)),
            Point(x+(int)(size*.65), y+(int)(size*.10)),
            Point(x+(int)(size*.70), y+(int)(size*.20)),
            Point(x+(int)(size*.70), y+(int)(size*.28)),
            Point(x+(int)(size*.30), y+(int)(size*.28))
        };
        g.FillPolygon(&gold, crown, 7);
        SolidBrush jewel(Color(255, 220, 20, 60));
        g.FillEllipse(&jewel, x+(int)(size*.47), y+(int)(size*.13), (int)(size*.06), (int)(size*.06));
    }
    else if (type == 5) {
        // Witch hat
        SolidBrush black(Color(255, 20, 20, 20)), purple(Color(255, 100, 20, 150));
        Point tip[] = {
            Point(x+(int)(size*.30), y+(int)(size*.22)),
            Point(x+(int)(size*.70), y+(int)(size*.22)),
            Point(x+(int)(size*.50), y+(int)(size*.02))
        };
        g.FillPolygon(&black, tip, 3);
        g.FillRectangle(&purple, x+(int)(size*.26), y+(int)(size*.20), (int)(size*.48), (int)(size*.06));
    }
}

// ── Apply color theme (returns ImageAttributes if theme active) ─
static bool BuildThemeAttr(ImageAttributes& attr) {
    bool isNight = false;
    time_t rawtime; struct tm ti;
    time(&rawtime);
    if (localtime_s(&ti, &rawtime) == 0) {
        int h = ti.tm_hour;
        isNight = (h >= 20 || h < 6);
    }

    if (g_colorTheme == 1) {
        ColorMatrix m = {
            0.393f,0.349f,0.272f,0.0f,0.0f,
            0.769f,0.686f,0.534f,0.0f,0.0f,
            0.189f,0.168f,0.131f,0.0f,0.0f,
            0.0f,  0.0f,  0.0f,  1.0f,0.0f,
            0.0f,  0.0f,  0.0f,  0.0f,1.0f
        };
        attr.SetColorMatrix(&m, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
        return true;
    }
    if (g_colorTheme == 2) {
        ColorMatrix m = {
            1.3f,0.0f,0.3f,0.0f,0.0f,
            0.0f,0.5f,0.0f,0.0f,0.0f,
            0.3f,0.0f,1.3f,0.0f,0.0f,
            0.0f,0.0f,0.0f,1.0f,0.0f,
            0.0f,0.0f,0.0f,0.0f,1.0f
        };
        attr.SetColorMatrix(&m, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
        return true;
    }
    if (g_colorTheme == 3) {
        ColorMatrix m = {
            0.0f,0.0f,0.0f,0.0f,0.0f,
            0.0f,1.2f,0.0f,0.0f,0.0f,
            0.0f,0.0f,1.5f,0.0f,0.0f,
            0.0f,0.0f,0.0f,0.6f,0.0f,
            0.0f,0.0f,0.0f,0.0f,1.0f
        };
        attr.SetColorMatrix(&m, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
        return true;
    }
    if (isNight) {
        ColorMatrix m = {
            0.5f,0.0f,0.0f,0.0f,0.0f,
            0.0f,0.6f,0.0f,0.0f,0.0f,
            0.0f,0.0f,1.0f,0.0f,0.0f,
            0.0f,0.0f,0.0f,0.9f,0.0f,
            0.0f,0.0f,0.0f,0.0f,1.0f
        };
        attr.SetColorMatrix(&m, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
        return true;
    }
    return false;
}

// ── Main render function ──────────────────────────────────────
void UpdateNekoWindow() {
    if (!g_hMainWnd || !g_bVisible) return;

    if (g_settings.hideOnFullscreen && IsFullscreenWindowActive()) {
        ShowWindow(g_hMainWnd, SW_HIDE);
        return;
    } else {
        ShowWindow(g_hMainWnd, SW_SHOWNOACTIVATE);
    }

    int catSize      = 32 * g_settings.size / 100;
    int shadowOffset = g_settings.showShadow ? 8 : 0;
    const int extraTop  = 90;
    const int extraSide = 80;
    int winWidth  = catSize + shadowOffset + (extraSide * 2);
    int winHeight = catSize + shadowOffset + extraTop;

    // ── Create memory DC ──────────────────────────────────────
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC    = CreateCompatibleDC(hScreenDC);

    BITMAPINFO bmi   = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = winWidth;
    bmi.bmiHeader.biHeight      = -winHeight;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap    = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    // ── GDI+ draw ─────────────────────────────────────────────
    Graphics g(hMemDC);
    g.SetCompositingMode(CompositingModeSourceCopy);
    SolidBrush clear(Color(0, 0, 0, 0));
    g.FillRectangle(&clear, 0, 0, winWidth, winHeight);
    g.SetCompositingMode(CompositingModeSourceOver);

    // Current frame
    Frame frame   = GetCharacterFrame(g_currentState, g_currentFrameIndex, g_settings.character);
    int   frameIdx = frame.row * 8 + frame.col;

    if (frameIdx >= 0 && frameIdx < 48 &&
        g_pSpriteFrames[frameIdx] &&
        g_pSpriteFrames[frameIdx]->GetLastStatus() == Ok)
    {
        Bitmap* pBmp = g_pSpriteFrames[frameIdx];

        // Shadow
        if (g_settings.showShadow) {
            ImageAttributes shadowAttr;
            ColorMatrix sm = {
                0.0f,0.0f,0.0f,0.0f,0.0f,
                0.0f,0.0f,0.0f,0.0f,0.0f,
                0.0f,0.0f,0.0f,0.0f,0.0f,
                0.0f,0.0f,0.0f,0.4f,0.0f,
                0.0f,0.0f,0.0f,0.0f,1.0f
            };
            shadowAttr.SetColorMatrix(&sm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
            g.DrawImage(pBmp, Rect(extraSide+shadowOffset, extraTop+shadowOffset, catSize, catSize),
                        0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel, &shadowAttr);
        }

        // Theme / night tint
        ImageAttributes themeAttr;
        bool hasTheme = BuildThemeAttr(themeAttr);

        // Cat sprite (with toss-spin if falling fast)
        if (g_isFalling && (std::abs(g_velocityX) > 2.0 || std::abs(g_velocityY) > 2.0)) {
            static float angle = 0.0f;
            angle += (float)(g_velocityX * 1.5);
            g.TranslateTransform(extraSide + catSize / 2.0f, extraTop + catSize / 2.0f);
            g.RotateTransform(angle);
            g.DrawImage(pBmp, Rect(-catSize/2, -catSize/2, catSize, catSize),
                        0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel, hasTheme ? &themeAttr : NULL);
            g.ResetTransform();
            g.TranslateTransform(extraSide + catSize / 2.0f, extraTop + catSize / 2.0f);
            g.RotateTransform(angle);
            DrawAccessory(g, -catSize/2, -catSize/2, catSize, g_settings.accessory);
            g.ResetTransform();
        } else {
            g.DrawImage(pBmp, Rect(extraSide, extraTop, catSize, catSize),
                        0, 0, pBmp->GetWidth(), pBmp->GetHeight(), UnitPixel, hasTheme ? &themeAttr : NULL);
            DrawAccessory(g, extraSide, extraTop, catSize, g_settings.accessory);
        }
    }

    // ── Feeding fish ──────────────────────────────────────────
    if (g_isFeeding) {
        SolidBrush fishBrush(Color(255, 60, 150, 240));
        Font fishFont(L"Segoe UI Emoji", 14, FontStyleRegular);
        g.DrawString(L"\U0001F41F", -1, &fishFont,
                     PointF((float)(g_fishX + extraSide), (float)g_fishY), &fishBrush);
    }

    // ── Particles ─────────────────────────────────────────────
    if (!g_particles.empty()) {
        Font pFont(L"Segoe UI Emoji", 9, FontStyleBold);
        for (const auto& p : g_particles) {
            if (p.symbol == 'Z') {
                SolidBrush zb(Color(255, 130, 180, 255));
                Font zf(L"Segoe UI", (REAL)(6 + (p.life % 4)), FontStyleBold);
                wchar_t str[2] = { p.symbol, L'\0' };
                g.DrawString(str, -1, &zf, PointF((float)(p.x + extraSide), (float)p.y), &zb);
            } else {
                SolidBrush hb(Color(255, 255, 80, 120));
                wchar_t str[2] = { p.symbol, L'\0' };
                g.DrawString(str, -1, &pFont, PointF((float)(p.x + extraSide), (float)p.y), &hb);
            }
        }
    }

    // ── Features (speech bubbles, toys, name tag) ─────────────
    DrawFeatures(g, extraSide, extraTop, catSize);

    // ── UpdateLayeredWindow ───────────────────────────────────
    POINT  ptDst   = { (int)g_nekoX - extraSide, (int)g_nekoY - extraTop };
    SIZE   sizeDst = { winWidth, winHeight };
    POINT  ptSrc   = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = (BYTE)(g_settings.opacity * 255 / 100);
    blend.AlphaFormat         = AC_SRC_ALPHA;
    UpdateLayeredWindow(g_hMainWnd, hScreenDC, &ptDst, &sizeDst, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    // ── Tray tooltip – quest progress ─────────────────────────
    NOTIFYICONDATAW nid = {};
    nid.cbSize   = sizeof(NOTIFYICONDATAW);
    nid.hWnd     = g_hMainWnd;
    nid.uID      = 1;
    nid.uFlags   = NIF_TIP;
    wcscpy_s(nid.szTip, GetQuestsStatusString().c_str());
    Shell_NotifyIconW(NIM_MODIFY, &nid);

    // ── Cleanup ───────────────────────────────────────────────
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);
}

// ── ApplyWindowSettings ───────────────────────────────────────
void ApplyWindowSettings() {
    if (!g_hMainWnd) return;

    if (g_settings.character != g_lastLoadedChar) {
        LoadCharacterSprite(g_settings.character);
        g_lastLoadedChar = g_settings.character;
    }

    HWND insertAfter = g_settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(g_hMainWnd, insertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    LONG_PTR exStyle = GetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE);
    if (g_settings.clickThrough) {
        if (!(GetAsyncKeyState(VK_CONTROL) & 0x8000))
            exStyle |= WS_EX_TRANSPARENT;
        else
            exStyle &= ~WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(g_hMainWnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(g_hMainWnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    int interval = 250 - (g_settings.speed * 6);
    if (interval < 50) interval = 50;
    SetTimer(g_hMainWnd, 1, interval, NULL);
}
