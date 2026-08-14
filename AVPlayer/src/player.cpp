#include "player.h"
#include "audio_decoder.h"
#include "video_decoder.h"
#include "../../CustomMetadata/custom_data.h"

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
    dataPlayer_ = std::make_shared<DataPlayer>(ctx_);

    if (demuxer_->Open() < 0) {
        av_log(nullptr, AV_LOG_ERROR, "demuxer open failed: %s\n", filename);
        return -1;
    }

    // 读取 MP4 全局 metadata 中的自定义数据(格式由 CUSTOM_DATA_FORMAT 宏控制)
    CustomData data = ParseCustomData(ctx_->fmtCtx_);
    ctx_->hasCustomData_ = data.hasData;
    ctx_->usrName_ = data.usrName;
    ctx_->usrCompany_ = data.usrCompany;
    ctx_->usrType_ = data.usrType;

    if (audioDecoder_->Open() < 0) {
        av_log(nullptr, AV_LOG_WARNING, "audio decoder open failed, continuing without audio\n");
    }

    if (videoDecoder_->Open() < 0) {
        av_log(nullptr, AV_LOG_WARNING, "video decoder open failed, continuing without video\n");
    }

    if (audioPlayer_->Open() < 0) {
        av_log(nullptr, AV_LOG_WARNING, "audio player open failed\n");
    }

    if (videoPlayer_->Open() < 0) {
        av_log(nullptr, AV_LOG_ERROR, "video player open failed\n");
        Close();
        return -1;
    }

    // DATA 流为可选,无 DATA 流时 Open 返回 -1(仅 info 日志),不影响播放
    if (dataPlayer_->Open() < 0) {
        av_log(nullptr, AV_LOG_INFO, "data player open skipped (no DATA stream)\n");
    }

    return 0;
}

void Player::Start() {
    demuxer_->Start();
    audioDecoder_->Start();
    videoDecoder_->Start();
    audioPlayer_->Start();
    dataPlayer_->Start();
    // m_video_player->start();
}

void Player::Close() {
	if (videoPlayer_) videoPlayer_->Close();
    if (audioPlayer_) audioPlayer_->Close();
    if (dataPlayer_) dataPlayer_->Close();
    if (audioDecoder_) audioDecoder_->Close();
    if (videoDecoder_) videoDecoder_->Close();
    if (demuxer_) demuxer_->Close();

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

double Player::Refresh() {
    return videoPlayer_->Refresh();
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