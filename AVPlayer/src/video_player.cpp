#include "video_player.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/time.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include "../../CustomMetadata/custom_data.h"
#include <string>

static const struct TextureFormatEntry {
    enum AVPixelFormat format;
    int texture_fmt;
} g_SdlTextureFormatMap[] = {
    { AV_PIX_FMT_RGB8,           SDL_PIXELFORMAT_RGB332 },
    { AV_PIX_FMT_RGB444,         SDL_PIXELFORMAT_RGB444 },
    { AV_PIX_FMT_RGB555,         SDL_PIXELFORMAT_RGB555 },
    { AV_PIX_FMT_BGR555,         SDL_PIXELFORMAT_BGR555 },
    { AV_PIX_FMT_RGB565,         SDL_PIXELFORMAT_RGB565 },
    { AV_PIX_FMT_BGR565,         SDL_PIXELFORMAT_BGR565 },
    { AV_PIX_FMT_RGB24,          SDL_PIXELFORMAT_RGB24 },
    { AV_PIX_FMT_BGR24,          SDL_PIXELFORMAT_BGR24 },
    { AV_PIX_FMT_0RGB32,         SDL_PIXELFORMAT_RGB888 },
    { AV_PIX_FMT_0BGR32,         SDL_PIXELFORMAT_BGR888 },
    { AV_PIX_FMT_NE(RGB0, 0BGR), SDL_PIXELFORMAT_RGBX8888 },
    { AV_PIX_FMT_NE(BGR0, 0RGB), SDL_PIXELFORMAT_BGRX8888 },
    { AV_PIX_FMT_RGB32,          SDL_PIXELFORMAT_ARGB8888 },
    { AV_PIX_FMT_RGB32_1,        SDL_PIXELFORMAT_RGBA8888 },
    { AV_PIX_FMT_BGR32,          SDL_PIXELFORMAT_ABGR8888 },
    { AV_PIX_FMT_BGR32_1,        SDL_PIXELFORMAT_BGRA8888 },
    { AV_PIX_FMT_YUV420P,        SDL_PIXELFORMAT_IYUV },
    { AV_PIX_FMT_YUVJ420P,       SDL_PIXELFORMAT_IYUV },
    { AV_PIX_FMT_YUYV422,        SDL_PIXELFORMAT_YUY2 },
    { AV_PIX_FMT_UYVY422,        SDL_PIXELFORMAT_UYVY },
    { AV_PIX_FMT_NONE,           SDL_PIXELFORMAT_UNKNOWN },
};

static void GetSdlPixFmtAndBlendmode(int format, Uint32& sdlPixFmt, SDL_BlendMode& sdlBlendMode) {
    sdlPixFmt = SDL_PIXELFORMAT_UNKNOWN;
    sdlBlendMode = SDL_BLENDMODE_NONE;
    if (format == AV_PIX_FMT_RGB32 ||
        format == AV_PIX_FMT_RGB32_1 ||
        format == AV_PIX_FMT_BGR32 ||
        format == AV_PIX_FMT_BGR32_1) {
        sdlBlendMode = SDL_BLENDMODE_BLEND;
    }
    for (int i = 0; i < sizeof(g_SdlTextureFormatMap) / sizeof(TextureFormatEntry); i++) {
        if (format == g_SdlTextureFormatMap[i].format) {
            sdlPixFmt = g_SdlTextureFormatMap[i].texture_fmt;
            break;
        }
    }
    return;
}

static Uint32 callback(Uint32 internal, void* param) {
    if (!param) {
        return 0;
    }
    VideoPlayer* player = static_cast<VideoPlayer*>(param);
    return player->Run(internal);
}

VideoPlayer::VideoPlayer(std::shared_ptr<Context> ctx)
    : ctx_(ctx) {

}

int VideoPlayer::Open() {
    window_ = SDL_CreateWindow("ffplayer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOW_DEFAULT_WIDTH,
        SDL_WINDOW_DEFAULT_HEIGHT,
        SDL_WINDOW_RESIZABLE);
    if (!window_) {
		av_log(nullptr, AV_LOG_ERROR, "SDL_CreateWindow failed\n");
        return -1;
    }
    SDL_StopTextInput();
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        renderer_ = SDL_CreateRenderer(window_, -1, 0);
    }
    if (!renderer_) {
		av_log(nullptr, AV_LOG_ERROR, "SDL_CreateRenderer failed\n");
        exit(1);
    }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer_, &info);

    SDL_RenderSetLogicalSize(renderer_, dstWidth_, dstHeight_);
    SDL_Rect viewport = { 0, 0, dstWidth_, dstHeight_ };
    SDL_RenderSetViewport(renderer_, &viewport);

    // 初始化 TTF 渲染器，并从 Context 中设置叠加数据
    ttfRenderer_.Init();
    if (ctx_->hasCustomData_) {
        ttfRenderer_.SetOverlayData(ctx_->usrName_, ctx_->usrCompany_, ctx_->usrType_);
    }
    return 0;
}

int VideoPlayer::Start() {
 //   if (m_timer_id != 0) {
	//	av_log(nullptr, AV_LOG_WARNING, "this video player already started\n");
 //       return 0;
 //   }
 //   int internal = 1000.0 / av_q2d(m_ctx->video_frame_rate);
 //   m_timer_id = SDL_AddTimer(internal, callback, this);
 //   if (0 == m_timer_id) {
	//	av_log(nullptr, AV_LOG_ERROR, "add video player timer failed\n");
 //       return -1;
 //   }
	//av_log(nullptr, AV_LOG_INFO, "start video player, internal=%d\n", internal);
    return 0;
}

int VideoPlayer::Close() {
    ttfRenderer_.Destroy();
    return 0;
}

void VideoPlayer::UpdateWidthHeight(int width, int height) {
    dstWidth_ = width;
    dstHeight_ = height;
    SDL_RenderSetLogicalSize(renderer_, width, height);
    SDL_Rect viewport = { 0, 0, width, height };
    SDL_RenderSetViewport(renderer_, &viewport);
}

void VideoPlayer::ToggleFullScreen() {
    isFullScreen_ = !isFullScreen_;
    av_log(nullptr, AV_LOG_INFO, "toggle fullscreen, fullscreen=%d\n", isFullScreen_);
    SDL_SetWindowFullscreen(window_, isFullScreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    ctx_->forceRefresh_ = 1;
}

int VideoPlayer::Run(int internal) {
    double remainingTime = Refresh();
    internal = (1000.0 / av_q2d(ctx_->videoFrameRate_)) / 2.0;
    return internal;
}

double VideoPlayer::Refresh() {
    double remainingTime = 0.0;
    if (ctx_->videoStream_) {
    retry:
        if (ctx_->videoFrameQueue_.Nb_Remaining() > 0) {
            Frame* lastvp = ctx_->videoFrameQueue_.PeekLast(); 
            Frame* vp = ctx_->videoFrameQueue_.Peek(); 
            
            if (vp->serial_ != ctx_->videoPacketQueue_.Serial()) {
                ctx_->videoFrameQueue_.Next();
                goto retry;
            }
           
            if (lastvp->serial_ != vp->serial_) {
                ctx_->videoFrameTimer_ = av_gettime_relative() / 1000000.0;
            }
           
            if (ctx_->paused_) {
                goto display;
            }
            
            double lastDuration = lastvp->VfDuration(vp, ctx_->maxFrameDuration_);
            double delay = ComputeTargetDelay(lastDuration);

            
            double time = av_gettime_relative() / 1000000.0;
            
            if (time < ctx_->videoFrameTimer_ + delay) {
                
                remainingTime = FFMIN(ctx_->videoFrameTimer_ + delay - time, remainingTime);
                
                goto display;
            }
            
            ctx_->videoFrameTimer_ += delay;
            
            if (delay > 0 && time - ctx_->videoFrameTimer_ > AV_SYNC_THRESHOLD_MAX) {
                ctx_->videoFrameTimer_ = time;
            }
            
            ctx_->videoFrameQueue_.Lock();
            if (!isnan(vp->pts_)) {
                UpdateVideoPts(vp->pts_, vp->serial_);
            }
            ctx_->videoFrameQueue_.Unlock();

            
            if (ctx_->videoFrameQueue_.Nb_Remaining() > 1) { 
                Frame* nextvp = ctx_->videoFrameQueue_.PeekNext(); 
                double duration = vp->VfDuration(nextvp, ctx_->maxFrameDuration_); 
                
                if (/*!m_ctx->step &&*/ (g_framedrop > 0 || (g_framedrop && ctx_->masterClock_->SyncType() != SYNC_TYPE_VIDEO)) && time > ctx_->videoFrameTimer_ + duration) {
                    ctx_->frameDropsLate_++;
                    ctx_->videoFrameQueue_.Next(); 
                    goto retry;
                }
            }

            ctx_->videoFrameQueue_.Next();
            ctx_->forceRefresh_ = 1;

            // TODO
            // if (is->step && !is->paused)
            //  stream_toggle_pause(is);
        }
display:
        if (ctx_->forceRefresh_ && ctx_->videoFrameQueue_.RindexShown()) {
            Frame* vp = ctx_->videoFrameQueue_.PeekLast();
            if (vp && vp->frame_ && vp->frame_->data[0]) {
                Display();
            }
            else if (texture_ && vp) {
                RenderLastTexture(vp);
            }
        }
        ctx_->forceRefresh_ = 0;
    }
    return remainingTime;
}

void VideoPlayer::Display() {
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    Render();
    ttfRenderer_.RenderOverlay(renderer_);
    SDL_RenderPresent(renderer_);
}

void VideoPlayer::RenderLastTexture(Frame* vp) {
    SDL_RenderClear(renderer_);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(window_, &win_w, &win_h);

    if (win_w != dstWidth_ || win_h != dstHeight_) {
        dstWidth_ = win_w;
        dstHeight_ = win_h;
        SDL_RenderSetLogicalSize(renderer_, win_w, win_h);
        SDL_Rect viewport = { 0, 0, win_w, win_h };
        SDL_RenderSetViewport(renderer_, &viewport);
    }

    int src_w = vp->width_ > 0 ? vp->width_ : width_;
    int src_h = vp->height_ > 0 ? vp->height_ : height_;
    double scale = (src_w > 0 && src_h > 0) ? FFMIN((double)win_w / src_w, (double)win_h / src_h) : 1.0;
    int fit_w = (int)(src_w * scale);
    int fit_h = (int)(src_h * scale);
    int offset_x = (win_w - fit_w) / 2;
    int offset_y = (win_h - fit_h) / 2;

    SDL_Rect rect = { offset_x, offset_y, fit_w, fit_h };
    SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
    ttfRenderer_.RenderOverlay(renderer_);
    SDL_RenderPresent(renderer_);
}

void VideoPlayer::Render() {
    Frame* vp = ctx_->videoFrameQueue_.PeekLast();
    if (!vp || !vp->frame_ ||
        vp->frame_->width <= 0 ||
        vp->frame_->height <= 0 ||
        !vp->frame_->data[0]) {
		av_log(nullptr, AV_LOG_ERROR, "invalid frame pts=%f serial=%d\n", vp ? vp->pts_ : -1, vp ? vp->serial_ : -1);
        return;
    }

    int src_w = vp->frame_->width;
    int src_h = vp->frame_->height;
    double scale_x = (double)dstWidth_ / src_w;
    double scale_y = (double)dstHeight_ / src_h;
    double scale = FFMIN(scale_x, scale_y);
    int fit_w = (int)(src_w * scale);
    int fit_h = (int)(src_h * scale);
    int offset_x = (dstWidth_ - fit_w) / 2;
    int offset_y = (dstHeight_ - fit_h) / 2;

    if (vp->uploaded_) {
        if (fit_w == width_ && fit_h == height_) {
            offsetX_ = offset_x;
            offsetY_ = offset_y;
            SDL_Rect rect = { offset_x, offset_y, fit_w, fit_h };
            SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
            return;
        }
        vp->uploaded_ = 0;
    }

    int size = av_image_get_buffer_size(static_cast<AVPixelFormat>(vp->frame_->format), vp->frame_->width, vp->frame_->height, 1);


    if (!swsCtx_ || fit_w != width_ || fit_h != height_) {
        if (swsCtx_) {
            sws_freeContext(swsCtx_);
            swsCtx_ = nullptr;
        }
        swsCtx_ = sws_getContext(vp->frame_->width, vp->frame_->height, static_cast<AVPixelFormat>(vp->frame_->format),
            fit_w, fit_h, static_cast<AVPixelFormat>(vp->frame_->format),
            SWS_BILINEAR, NULL, NULL, NULL);
        if (!swsCtx_) {
			av_log(nullptr, AV_LOG_ERROR, "sws_getContext failed\n");
            return;
        }

        if (data_[0] != nullptr) {
            av_freep(&data_[0]);
        }
        dataSize_ = av_image_alloc(data_, lineSize_,
            fit_w, fit_h, static_cast<AVPixelFormat>(vp->frame_->format), 1);
        if (dataSize_ < 0) {
			av_log(nullptr, AV_LOG_ERROR, "av_image_alloc failed\n");
            return;
        }
        width_ = fit_w;
        height_ = fit_h;
    }

    sws_scale(swsCtx_, (const uint8_t* const*)vp->frame_->data, vp->frame_->linesize, 0,
        vp->frame_->height, data_, lineSize_);

    int ret = 0;
    Uint32 sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
    SDL_BlendMode sdl_blendmode = SDL_BLENDMODE_NONE;
    GetSdlPixFmtAndBlendmode(vp->frame_->format, sdl_pix_fmt, sdl_blendmode);
    CreateTexture(sdl_pix_fmt, width_, height_, sdl_blendmode);
    switch (sdl_pix_fmt) {
    case SDL_PIXELFORMAT_IYUV:
        if (lineSize_[0] > 0 &&
            lineSize_[1] > 0 &&
            lineSize_[2] > 0) {
            ret = SDL_UpdateYUVTexture(texture_, nullptr,
                data_[0], lineSize_[0],
                data_[1], lineSize_[1],
                data_[2], lineSize_[2]);

        }
        else if (lineSize_[0] < 0 &&
            lineSize_[1] < 0 &&
            lineSize_[2] < 0) {
            ret = SDL_UpdateYUVTexture(texture_, nullptr,
                data_[0] + lineSize_[0] * (height_ - 1), -lineSize_[0],
                data_[1] + lineSize_[1] * (AV_CEIL_RSHIFT(height_, 1) - 1), -lineSize_[1],
                data_[2] + lineSize_[2] * (AV_CEIL_RSHIFT(height_, 1) - 1), -lineSize_[2]);
        }
        else {
			av_log(nullptr, AV_LOG_ERROR, "Mixed negative and positive linesizes are not supported\n");
        }
        break;
    default:
        if (lineSize_[0] < 0) {
            ret = SDL_UpdateTexture(texture_, nullptr,
                data_[0] + lineSize_[0] * (height_ - 1), -lineSize_[0]);
        }
        else {
            ret = SDL_UpdateTexture(texture_, nullptr, data_[0], lineSize_[0]);
        }
        break;
    }
    vp->uploaded_ = 1;
    offsetX_ = offset_x;
    offsetY_ = offset_y;
    SDL_Rect rect = { offset_x, offset_y, width_, height_ };
    ret = SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
}

void VideoPlayer::CreateTexture(Uint32 format, int width, int height, SDL_BlendMode blendmode) {
    bool recreate = true;
    Uint32 fmt = SDL_PIXELFORMAT_UNKNOWN;
    int access = 0, w = 0, h = 0;
    if (texture_ &&
        SDL_QueryTexture(texture_, &fmt, &access, &w, &h) >= 0
        && fmt == format && w == width && h == height) {
        return;
    }
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    texture_ = SDL_CreateTexture(renderer_, format == SDL_PIXELFORMAT_UNKNOWN ? SDL_PIXELFORMAT_ARGB8888 : format, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture_) {
		av_log(nullptr, AV_LOG_ERROR, "SDL_CreateTexture failed\n");
        return;
    }

    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) >= 0) {
        memset(pixels, 0, pitch * height);
        SDL_UnlockTexture(texture_);
    }

    if (SDL_SetTextureBlendMode(texture_, blendmode) < 0) {
		av_log(nullptr, AV_LOG_ERROR, "SDL_SetTextureBlendMode failed\n");
        return;
    }

    SDL_RenderSetLogicalSize(renderer_, dstWidth_, dstHeight_);
    SDL_Rect viewport = { 0, 0, dstWidth_, dstHeight_ };
    SDL_RenderSetViewport(renderer_, &viewport);
}

double VideoPlayer::ComputeTargetDelay(double delay) {
    double delay0 = delay;
    if (ctx_->masterClock_->SyncType() != SYNC_TYPE_VIDEO) {
        
        double diff = ctx_->videoClock_.Get() - ctx_->masterClock_->Get();

        double syncThreshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));

        if (!isnan(diff) && fabs(diff) < ctx_->maxFrameDuration_) {
            if (diff <= -syncThreshold) { 
                delay = FFMAX(0, delay + diff); 
            }
            else if (diff >= syncThreshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) { 
                delay = delay + diff; 
            }
            else if (diff >= syncThreshold) { 
                delay = 2 * delay; 
            }
        }
    };
    return delay;
}

void VideoPlayer::UpdateVideoPts(double pts, int serial) {
    ctx_->videoClock_.Set(pts, serial);
    // m_ctx->video_clock.sync_from_slave(m_ctx->extern_clock);
}