// audio.h – Sound system for Keday
#ifndef AUDIO_H
#define AUDIO_H

#include <string>

// Play procedural sine wave (blocking, call from thread)
void PlaySoundWave(double frequency, int durationMs, int volumePercent);

// Character-specific meow (async, non-blocking)
void PlayMeowAsync();

// Purring sound (async, non-blocking)
void PlayPurrAsync();

// Low-level character sound dispatcher
void PlayCharacterSoundAsync(const std::wstring& character, int volume);

#endif // AUDIO_H
