#include "audio_player.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/time.h>
#include <libavutil/opt.h>

#ifdef __cplusplus
}
#endif

static void callback(void* opaque, Uint8* stream, int len) {
    if (!opaque) {
        return;
    }
    AudioPlayer* player = static_cast<AudioPlayer*>(opaque);
    return player->Run(stream, len);
}

AudioPlayer::AudioPlayer(std::shared_ptr<Context> ctx)
    : ctx_(ctx) {

}

int AudioPlayer::Open() {
    if (!ctx_->audioCodecCtx_) {
        av_log(nullptr, AV_LOG_WARNING, "audioCodecCtx is null, cannot open audio player\n");
        return -1;
    }
    SDL_AudioSpec desired, obtained;
    desired.channels = ctx_->audioCodecCtx_->ch_layout.nb_channels;
    desired.freq = ctx_->audioCodecCtx_->sample_rate;
    desired.format = AUDIO_S16SYS;
    desired.silence = 0;
    desired.samples = std::max(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(desired.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
    desired.callback = callback;
    desired.userdata = this;
    audioDevId_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (0 == audioDevId_) {
		av_log(nullptr, AV_LOG_ERROR, "SDL_OpenAudio failed: %s\n", SDL_GetError());
    }

    audioHwBufSize_ = obtained.size;
    bytesPerSec_ = av_samples_get_buffer_size(nullptr, desired.channels, desired.freq, AV_SAMPLE_FMT_S16, 1);
    return audioDevId_;
}

int AudioPlayer::Start() {
    SDL_PauseAudioDevice(audioDevId_, 0);
    return 0;
}

int AudioPlayer::Stop() {
    SDL_PauseAudioDevice(audioDevId_, 1);
    return 0;
}

int AudioPlayer::Close() {
    ctx_->audioFrameQueue_.Abort();
    Stop();
    SDL_CloseAudioDevice(audioDevId_);
    audioDevId_ = -1;

    if (currentAudioBufData_) {
        av_freep(&currentAudioBufData_);
        currentAudioBufSize_ = 0;
    }
    return 0;
}

void AudioPlayer::UpdateVolume(int volume) {
    int temp = volume_ + volume;
    if (temp > MAX_VOLUME_VALUE) {
        temp = MAX_VOLUME_VALUE;
    }
    else if (temp < MIN_VOLUME_VALUE) {
        temp = MIN_VOLUME_VALUE;
    }
    volume_ = temp;
	av_log(nullptr, AV_LOG_INFO, "volume=%d\n", temp);
}

void AudioPlayer::ToggleMute() {
    muted_ = !muted_;
    av_log(nullptr, AV_LOG_INFO, "toggle mute, muted=%d\n", muted_.load());
}

bool AudioPlayer::IsMuted() const {
    return muted_;
}

void AudioPlayer::Run(Uint8* stream, int len) {
    int len1 = 0;
    int64_t audio_callback_time = av_gettime_relative();

    while (len > 0) {
        
        if (currentAudioBufIndex_ >= currentAudioBufSize_) {
            if (GetAudioData() < 0) {
                currentAudioBufData_ = nullptr;
                currentAudioBufSize_ = SDL_AUDIO_MIN_BUFFER_SIZE;
            }
            // TODO is->show_mode != SHOW_MODE_VIDEO
            
            // m_current_audio_buf_size = data.length;
            // m_current_audio_buf_index = 0;
        }
        
        len1 = currentAudioBufSize_ - currentAudioBufIndex_;
        if (len1 > len) {
            len1 = len;
        }
        if (!muted_ && currentAudioBufData_ && SDL_MIX_MAXVOLUME == volume_) {
            memcpy(stream, (uint8_t*)currentAudioBufData_ + currentAudioBufIndex_, len1);
        }
        else {
            memset(stream, 0, len1);
            if (!muted_ && currentAudioBufData_) {
                SDL_MixAudioFormat(stream, currentAudioBufData_ + currentAudioBufIndex_, AUDIO_S16SYS, len1, volume_);
            }
        }
        len -= len1;
        stream += len1;
        currentAudioBufIndex_ += len1;
    }

    int audio_write_buf_size = currentAudioBufSize_ - currentAudioBufIndex_;
    if (!isnan(currentAudioClock_)) {
        ctx_->audioClock_.Set_at(currentAudioClock_ - (double)(2 * audioHwBufSize_ + audio_write_buf_size) / bytesPerSec_, currentAudioClockSerial_, audio_callback_time / 1000000.0);
    }
    return;
}

int AudioPlayer::GetAudioData() {
    currentAudioBufIndex_ = 0;
    if (ctx_->paused_) {
        return -1;
    }
    Frame* af = nullptr;
    do {
        af = ctx_->audioFrameQueue_.PeekReadable();

        if (!af || !af->frame_) {
            //av_log(nullptr, AV_LOG_ERROR,"audio frame null\n");
            return -1;
        }
        //av_log(nullptr,
        //    AV_LOG_INFO,
        //    "audio player get frame samples=%d channels=%d serial=%d\n",
        //    af->frame->nb_samples,
        //    af->frame->ch_layout.nb_channels,
        //    af->serial);
        ctx_->audioFrameQueue_.Next();
    } while (af->serial_ != ctx_->audioPacketQueue_.Serial());

    if (!ctx_->audioSwrCtx_) {
        ctx_->audioSwrCtx_ = swr_alloc();
        if (!ctx_->audioSwrCtx_) {
			av_log(nullptr, AV_LOG_ERROR, "swr_alloc failed\n");
            return -1;
        }

        //swr_alloc_set_opts2(&m_ctx->audio_swr_ctx,
        //    &af->frame->ch_layout, AV_SAMPLE_FMT_S16, af->frame->sample_rate,
        //    &af->frame->ch_layout, static_cast<AVSampleFormat>(af->frame->format), af->frame->sample_rate,
        //    0, nullptr);
        av_opt_set_chlayout(ctx_->audioSwrCtx_, "in_chlayout", &af->frame_->ch_layout, 0);
        av_opt_set_int(ctx_->audioSwrCtx_, "in_sample_rate", af->frame_->sample_rate, 0);
        av_opt_set_sample_fmt(ctx_->audioSwrCtx_, "in_sample_fmt", static_cast<AVSampleFormat>(af->frame_->format), 0);

        av_opt_set_chlayout(ctx_->audioSwrCtx_, "out_chlayout", &af->frame_->ch_layout, 0);
        av_opt_set_int(ctx_->audioSwrCtx_, "out_sample_rate", af->frame_->sample_rate, 0);
        av_opt_set_sample_fmt(ctx_->audioSwrCtx_, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

        if (swr_init(ctx_->audioSwrCtx_) < 0) {
			av_log(nullptr, AV_LOG_ERROR, "swr_init failed\n");
            return -1;
        }
    }

    if (ctx_->audioSwrCtx_)
    {
        const uint8_t** in = (const uint8_t**)af->frame_->extended_data;
        uint8_t** out = &currentAudioBufData_;
        int dataSize = av_samples_get_buffer_size(nullptr,
            af->frame_->ch_layout.nb_channels,
            af->frame_->nb_samples,
            AV_SAMPLE_FMT_S16,
            0);
		//av_log(nullptr, AV_LOG_INFO, "audio malloc size=%d\n", dataSize);
        av_fast_malloc(&currentAudioBufData_, &currentAudioBufSize_, dataSize);
        if (!currentAudioBufData_) {
			av_log(nullptr, AV_LOG_ERROR, "av_fast_malloc failed\n");
            return -1;
        }
        int len = swr_convert(ctx_->audioSwrCtx_, out, af->frame_->nb_samples, in, af->frame_->nb_samples);
        if (len < 0) {
			av_log(nullptr, AV_LOG_ERROR, "swr_convert failed\n");
            return -1;
        }

        currentAudioBufSize_ = av_samples_get_buffer_size(nullptr, af->frame_->ch_layout.nb_channels,
            len, AV_SAMPLE_FMT_S16, 1);
    }
    else {
        int data_size = av_samples_get_buffer_size(nullptr,
            af->frame_->ch_layout.nb_channels,
            af->frame_->nb_samples,
            static_cast<AVSampleFormat>(af->frame_->format),
            1);
        currentAudioBufData_ = af->frame_->data[0];
        currentAudioBufSize_ = data_size;
    }


    // TODO synchronize_audio

    // TODO af->frame->format != audio_src.fmt

    // TODO swr_ctx
    if (!isnan(af->pts_)) {
        currentAudioClock_ = af->pts_ + (double)af->frame_->nb_samples / af->frame_->sample_rate;
    }
    else {
        currentAudioClock_ = NAN;
    }
    currentAudioClockSerial_ = af->serial_;

    lastAudioClock_ = currentAudioClock_;
    return 0;
}