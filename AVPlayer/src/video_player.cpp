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

    ttfRenderer_.Init();

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
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (data_[0]) {
        av_freep(&data_[0]);
    }
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
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
    double remainingTime = 0.01;
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

    Frame* vp = ctx_->videoFrameQueue_.PeekLast();
    UpdateDataOverlay(vp);
    ttfRenderer_.RenderStreamOverlay(renderer_);

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

    UpdateDataOverlay(vp);
    ttfRenderer_.RenderStreamOverlay(renderer_);

    SDL_RenderPresent(renderer_);
}

void VideoPlayer::UpdateDataOverlay(Frame* vp) {
    if (ctx_->dataIndex_ < 0) {
        return;
    }
    if (ctx_->dataFrameQueue_.Nb_Remaining() <= 0) {
        return;
    }

    while (ctx_->dataFrameQueue_.Nb_Remaining() > 0) {
        Frame* sp = ctx_->dataFrameQueue_.Peek();
        Frame* sp1 = (ctx_->dataFrameQueue_.Nb_Remaining() > 1) ? ctx_->dataFrameQueue_.PeekNext() : nullptr;

        bool staleSerial = (sp->serial_ != ctx_->dataPacketQueue_.Serial());
        bool nextReady = (sp1 && vp && !isnan(vp->pts_) && vp->pts_ >= sp1->pts_);

        if (staleSerial || nextReady) {
            ctx_->dataFrameQueue_.Next();
        } else {
            break;
        }
    }

    if (ctx_->dataFrameQueue_.Nb_Remaining() <= 0) {
        return;
    }

    Frame* sp = ctx_->dataFrameQueue_.Peek();
    if (!vp || isnan(vp->pts_) || vp->pts_ < sp->pts_) {
        return;
    }

    ttfRenderer_.SetStreamLines(sp->dataLines_);
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
    int fit_w, fit_h, offset_x, offset_y;
    CalcDisplayRect(src_w, src_h, fit_w, fit_h, offset_x, offset_y);

    Uint32 sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
    SDL_BlendMode sdl_blendmode = SDL_BLENDMODE_NONE;
    GetSdlPixFmtAndBlendmode(vp->frame_->format, sdl_pix_fmt, sdl_blendmode);

    if (sdl_pix_fmt != SDL_PIXELFORMAT_UNKNOWN) {
        RenderDirectUpload(vp, sdl_pix_fmt, sdl_blendmode, src_w, src_h, fit_w, fit_h, offset_x, offset_y);
    } else {
        RenderSwsConvert(vp, sdl_pix_fmt, sdl_blendmode, src_w, src_h, fit_w, fit_h, offset_x, offset_y);
    }
}

void VideoPlayer::CalcDisplayRect(int src_w, int src_h, int& fit_w, int& fit_h, int& offset_x, int& offset_y) {
    double scale_x = (double)dstWidth_ / src_w;
    double scale_y = (double)dstHeight_ / src_h;
    double scale = FFMIN(scale_x, scale_y);
    fit_w = (int)(src_w * scale);
    fit_h = (int)(src_h * scale);
    offset_x = (dstWidth_ - fit_w) / 2;
    offset_y = (dstHeight_ - fit_h) / 2;
}

void VideoPlayer::UploadTexture(Uint32 sdl_pix_fmt, uint8_t* const* data, const int* linesize, int height) {
    switch (sdl_pix_fmt) {
    case SDL_PIXELFORMAT_IYUV:
        if (linesize[0] > 0 && linesize[1] > 0 && linesize[2] > 0) {
            SDL_UpdateYUVTexture(texture_, nullptr,
                data[0], linesize[0],
                data[1], linesize[1],
                data[2], linesize[2]);
        }
        else if (linesize[0] < 0 && linesize[1] < 0 && linesize[2] < 0) {
            SDL_UpdateYUVTexture(texture_, nullptr,
                data[0] + linesize[0] * (height - 1), -linesize[0],
                data[1] + linesize[1] * (AV_CEIL_RSHIFT(height, 1) - 1), -linesize[1],
                data[2] + linesize[2] * (AV_CEIL_RSHIFT(height, 1) - 1), -linesize[2]);
        }
        else {
            av_log(nullptr, AV_LOG_ERROR, "Mixed negative and positive linesizes are not supported\n");
        }
        break;
    default:
        if (linesize[0] < 0) {
            SDL_UpdateTexture(texture_, nullptr,
                data[0] + linesize[0] * (height - 1), -linesize[0]);
        }
        else {
            SDL_UpdateTexture(texture_, nullptr, data[0], linesize[0]);
        }
        break;
    }
}

void VideoPlayer::RenderDirectUpload(Frame* vp, Uint32 sdl_pix_fmt, SDL_BlendMode sdl_blendmode,
                                int src_w, int src_h, int fit_w, int fit_h, int offset_x, int offset_y) {
    if (vp->uploaded_ && lastPathWasGpu_ && src_w == width_ && src_h == height_) {
        offsetX_ = offset_x;
        offsetY_ = offset_y;
        SDL_Rect rect = { offset_x, offset_y, fit_w, fit_h };
        SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
        return;
    }
    vp->uploaded_ = 0;

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    if (data_[0] != nullptr) {
        av_freep(&data_[0]);
    }

    CreateTexture(sdl_pix_fmt, src_w, src_h, sdl_blendmode);
    UploadTexture(sdl_pix_fmt, vp->frame_->data, vp->frame_->linesize, src_h);

    vp->uploaded_ = 1;
    width_ = src_w;
    height_ = src_h;
    offsetX_ = offset_x;
    offsetY_ = offset_y;
    SDL_Rect rect = { offset_x, offset_y, fit_w, fit_h };
    SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
    lastPathWasGpu_ = true;
}

void VideoPlayer::RenderSwsConvert(Frame* vp, Uint32 sdl_pix_fmt, SDL_BlendMode sdl_blendmode,
                                int src_w, int src_h, int fit_w, int fit_h, int offset_x, int offset_y) {
    if (vp->uploaded_ && !lastPathWasGpu_) {
        if (fit_w == width_ && fit_h == height_) {
            offsetX_ = offset_x;
            offsetY_ = offset_y;
            SDL_Rect rect = { offset_x, offset_y, fit_w, fit_h };
            SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
            return;
        }
        vp->uploaded_ = 0;
    }

    if (!swsCtx_ || fit_w != width_ || fit_h != height_) {
        if (swsCtx_) {
            sws_freeContext(swsCtx_);
            swsCtx_ = nullptr;
        }
        swsCtx_ = sws_getContext(src_w, src_h, static_cast<AVPixelFormat>(vp->frame_->format),
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

    CreateTexture(sdl_pix_fmt, width_, height_, sdl_blendmode);
    UploadTexture(sdl_pix_fmt, data_, lineSize_, height_);

    vp->uploaded_ = 1;
    offsetX_ = offset_x;
    offsetY_ = offset_y;
    SDL_Rect rect = { offset_x, offset_y, width_, height_ };
    SDL_RenderCopy(renderer_, texture_, nullptr, &rect);
    lastPathWasGpu_ = false;
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
    }
    return delay;
}

void VideoPlayer::UpdateVideoPts(double pts, int serial) {
    ctx_->videoClock_.Set(pts, serial);
    // m_ctx->video_clock.sync_from_slave(m_ctx->extern_clock);
}