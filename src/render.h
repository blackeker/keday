// render.h – GDI+ rendering for Keday
#ifndef RENDER_H
#define RENDER_H

#include <windows.h>
#include <gdiplus.h>
#include "app.h"

using namespace Gdiplus;

// Spawn floating heart / Zzz particles above the cat
void SpawnHearts(double startX, double startY, int count, wchar_t symbol = L'\x2665');

// Draw procedural accessory on top of the cat sprite
void DrawAccessory(Graphics& g, int x, int y, int size, int type);

// Full layered-window redraw (call after any state change)
void UpdateNekoWindow();

// Apply window settings (timer interval, always-on-top, click-through, sprite load)
void ApplyWindowSettings();

// Get working area of the monitor the cat is currently on
void GetNekoMonitorBounds(int x, int y, RECT& rect);

// Fullscreen detection helper
bool IsFullscreenWindowActive();

#endif // RENDER_H
