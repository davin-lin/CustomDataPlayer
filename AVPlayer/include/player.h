#pragma once

#include <string>
#include <memory>
#include "context.h"
#include "demuxer.h"
#include "decoder.h"
#include "video_player.h"
#include "audio_player.h"

#define SEEK_BY_BYTES 1

class Player {
public:
    Player();

    int Open(const char* filename);
    void Start();
    void Close();

    void Refresh();

    void TogglePause();
    void ToggleMute();
    void ToggleFullScreen();
    void UpdateWidthHeight(int width, int height);
    void ForceRefresh();

    void VolumeUp(int volume);
    void VolumeDown(int volume);
    void SeekForward(double incr, bool seekByBytes = SEEK_BY_BYTES);
    void SeekBackward(double incr, bool seekByBytes = SEEK_BY_BYTES);

    bool IsPaused() const;
    bool IsMuted() const;
private:
    std::shared_ptr<Context> ctx_;
    std::shared_ptr<Demuxer> demuxer_;
    std::shared_ptr<Decoder> audioDecoder_;
    std::shared_ptr<Decoder> videoDecoder_;
    std::shared_ptr<Decoder> subtitleDecoder_;
    std::shared_ptr<AudioPlayer> audioPlayer_;
    std::shared_ptr<VideoPlayer> videoPlayer_;
};