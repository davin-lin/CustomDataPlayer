#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <SDL_audio.h>
#ifdef __cplusplus
}
#endif

#include "context.h"

#define MIN_VOLUME_VALUE 0
#define DEFAULT_VOLUME_VALUE 64
#define MAX_VOLUME_VALUE 128

class AudioPlayer {
    friend void callback(void* opaque, Uint8* steram, int len);
public:
    AudioPlayer(std::shared_ptr<Context> ctx);
    int Open();
    int Start();
    int Stop();
    int Close();

    void ToggleMute();         // ¾²Òô
    void UpdateVolume(int volume); // ¸üÐÂÉùÒô

    bool IsMuted() const;
private:
    void Run(Uint8* steram, int len);
    int GetAudioData();
private:
    std::shared_ptr<Context> ctx_;
    SDL_AudioDeviceID audioDevId_ = -1;

    int audioHwBufSize_ = 0;
    int bytesPerSec_ = 0;

    uint8_t* currentAudioBufData_ = nullptr;
    unsigned int currentAudioBufSize_ = 0;
    int currentAudioBufIndex_ = 0;
    int currentAudioClockSerial_ = 0;
    double currentAudioClock_ = 0.0;
    double lastAudioClock_ = 0.0;

    std::atomic<int> volume_ = DEFAULT_VOLUME_VALUE;
    std::atomic<bool> muted_ = false;

};