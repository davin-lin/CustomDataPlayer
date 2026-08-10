#include "player.h"
#include "audio_decoder.h"
#include "video_decoder.h"

Player::Player()
{
    SDL_Init(SDL_INIT_EVERYTHING);
}

int Player::Open(const char* filename) {
    ctx_ = std::make_shared<Context>(filename);
    demuxer_ = std::make_shared<Demuxer>(ctx_);
    audioDecoder_ = std::make_shared<AudioDecoder>(ctx_);
    videoDecoder_ = std::make_shared<VideoDecoder>(ctx_);
    audioPlayer_ = std::make_shared<AudioPlayer>(ctx_);
    videoPlayer_ = std::make_shared<VideoPlayer>(ctx_);

    demuxer_->Open();
    audioDecoder_->Open();
    videoDecoder_->Open();
    audioPlayer_->Open();
    videoPlayer_->Open();

    return 0;
}

void Player::Start() {
    demuxer_->Start();
    audioDecoder_->Start();
    videoDecoder_->Start();
    audioPlayer_->Start();
    // m_video_player->start();
}

void Player::Close() {
    //m_video_player->close();

    audioPlayer_->Close();

    audioDecoder_->Close();

    videoDecoder_->Close();

    demuxer_->Close();

    SDL_Quit();
    av_log(nullptr, AV_LOG_INFO, "player closed\n");
}

void Player::TogglePause() {
    ctx_->paused_ = !ctx_->paused_;
    ctx_->pauseCond_.notify_one();
	av_log(nullptr, AV_LOG_INFO, "toggle pause, paused=%d\n", ctx_->paused_.load());
}

void Player::ToggleMute() {
    audioPlayer_->ToggleMute();
}

void Player::ToggleFullScreen() {
    videoPlayer_->ToggleFullScreen();
}

void Player::UpdateWidthHeight(int width, int height) {
    videoPlayer_->UpdateWidthHeight(width, height);
}

void Player::ForceRefresh() {
    ctx_->forceRefresh_ = 1;
}

bool Player::IsPaused() const {
    return ctx_->paused_;
}

bool Player::IsMuted() const {
    return audioPlayer_->IsMuted();
}

void Player::Refresh() {
    videoPlayer_->Run(0);
}

void Player::VolumeUp(int volume) {
    audioPlayer_->UpdateVolume(volume);
}
void Player::VolumeDown(int volume) {
    audioPlayer_->UpdateVolume(-volume);
}
void Player::SeekForward(double incr, bool seekByBytes) {
    demuxer_->Seek(incr, seekByBytes);
}
void Player::SeekBackward(double incr, bool seekByBytes) {
    demuxer_->Seek(incr, seekByBytes);
}