// audio.cpp – Sound system implementation for Keday
#define _USE_MATH_DEFINES
#include "audio.h"
#include "app.h"
#include <windows.h>
#include <math.h>
#include <vector>
#include <thread>

void PlaySoundWave(double frequency, int durationMs, int volumePercent) {
    if (volumePercent <= 0) return;

    const int sampleRate = 22050;
    int numSamples = (sampleRate * durationMs) / 1000;

#pragma pack(push,1)
    struct WAVHeader {
        char  chunkID[4]     = {'R','I','F','F'};
        int   chunkSize;
        char  format[4]      = {'W','A','V','E'};
        char  sub1ID[4]      = {'f','m','t',' '};
        int   sub1Size       = 16;
        short audioFormat    = 1;   // PCM
        short numChannels    = 1;   // Mono
        int   sampleRate;
        int   byteRate;
        short blockAlign     = 1;
        short bitsPerSample  = 8;
        char  sub2ID[4]      = {'d','a','t','a'};
        int   sub2Size;
    } header;
#pragma pack(pop)

    header.sampleRate = sampleRate;
    header.byteRate   = sampleRate;
    header.sub2Size   = numSamples;
    header.chunkSize  = 36 + numSamples;

    std::vector<char> wavData(sizeof(WAVHeader) + numSamples);
    double amplitude = (volumePercent / 100.0) * 127.0;
    char* pData = wavData.data() + sizeof(WAVHeader);

    for (int i = 0; i < numSamples; ++i) {
        double t     = (double)i / sampleRate;
        double angle = 2.0 * M_PI * frequency * t;
        // Apply fade-out for the last 20% to avoid clicks
        double fade  = (i > numSamples * 0.8)
                     ? (1.0 - (i - numSamples * 0.8) / (numSamples * 0.2))
                     : 1.0;
        pData[i] = (char)(128 + amplitude * fade * sin(angle));
    }
    memcpy(wavData.data(), &header, sizeof(WAVHeader));
    PlaySoundW((LPCWSTR)wavData.data(), NULL, SND_MEMORY | SND_SYNC | SND_NODEFAULT);
}

void PlayCharacterSoundAsync(const std::wstring& character, int volume) {
    // Only allow one sound at a time
    if (g_soundPlaying.exchange(true)) return;
    std::thread([character, volume]() {
        if (character == L"dog") {
            PlaySoundWave(200.0, 70, volume);
            Sleep(80);
            PlaySoundWave(230.0, 90, volume);
        } else if (character == L"sakura" || character == L"tomoyo") {
            PlaySoundWave(980.0,  50, volume);
            Sleep(60);
            PlaySoundWave(1300.0, 80, volume);
        } else if (character == L"bsd") {
            PlaySoundWave(400.0, 40, volume);
            Sleep(50);
            PlaySoundWave(600.0, 40, volume);
            Sleep(50);
            PlaySoundWave(800.0, 60, volume);
        } else {
            // Default: cute cat meow
            PlaySoundWave(650.0,  50, volume);
            Sleep(60);
            PlaySoundWave(850.0,  60, volume);
            Sleep(70);
            PlaySoundWave(1100.0, 120, volume);
        }
        g_soundPlaying = false;
    }).detach();
}

void PlayMeowAsync() {
    PlayCharacterSoundAsync(g_settings.character, g_settings.volume);
}

void PlayPurrAsync() {
    if (g_soundPlaying.exchange(true)) return;
    int vol = g_settings.volume;
    std::thread([vol]() {
        for (int i = 0; i < 3; ++i) {
            PlaySoundWave(160.0, 35, vol);
            Sleep(90);
        }
        g_soundPlaying = false;
    }).detach();
}
