// neko.h – Cat physics, state machine and audio meter
#ifndef NEKO_H
#define NEKO_H

#include <windows.h>
#include "app.h"

// Returns true if the cat's position or state changed (→ redraw needed)
bool UpdateNekoLogic();

// Window-ledge collision helpers
bool IsStandingOnWindowLedge(int x, int y, int catSize, int& outLedgeY);
bool IsSpanningWindowLedge(int x, int y, double velocityY, int catSize, int& outLedgeY);

// System audio peak (for music-dance mode)
float GetSystemAudioPeakVolume();

#endif // NEKO_H
