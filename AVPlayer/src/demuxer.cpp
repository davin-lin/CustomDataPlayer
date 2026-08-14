#include "demuxer.h"
#include "opts.h"

Demuxer::Demuxer(std::shared_ptr<Context> ctx)
    : ctx_(ctx)
{
}

Demuxer::~Demuxer() {

}

int Demuxer::Open() {
    int ret = avformat_open_input(&ctx_->fmtCtx_, ctx_->filename_, ctx_->iformat_, nullptr);
    if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "avformat_open_input failed, ret=%d\n", ret);
        return -1;
    }
    ret = avformat_find_stream_info(ctx_->fmtCtx_, nullptr);
    if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "avformat_find_stream_info failed, ret=%d\n", ret);
        return -1;
    }
    ctx_->maxFrameDuration_ = (ctx_->fmtCtx_->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;
    av_dump_format(ctx_->fmtCtx_, 0, ctx_->filename_, 0);
    return 0;
}

int Demuxer::Close() {
    return 0;
}

void Demuxer::Seek(double incr, int seekByBytes) {
    double pos = -1.0;
    if (seekByBytes && !(ctx_->fmtCtx_->iformat->flags & AVFMT_NO_BYTE_SEEK)) {
        if (pos < 0) {
            pos = ctx_->videoFrameQueue_.LastPos();
        }
        if (pos < 0) {
            pos = ctx_->audioFrameQueue_.LastPos();
        }
        if (pos < 0) {
            pos = avio_tell(ctx_->fmtCtx_->pb);
        }
        if (ctx_->fmtCtx_->bit_rate) {
            incr *= ctx_->fmtCtx_->bit_rate / 8.0;
        }
        else {
            incr *= 180000.0;
        }
        pos += incr;
    }
    else {
        pos = ctx_->masterClock_->Get();
        if (isnan(pos)) {
            pos = (double)ctx_->seekPos_ / AV_TIME_BASE;
        }
        pos += incr;

        if (ctx_->fmtCtx_->start_time != AV_NOPTS_VALUE &&
            pos < ctx_->fmtCtx_->start_time / (double)AV_TIME_BASE) {
            pos = ctx_->fmtCtx_->start_time / (double)AV_TIME_BASE;
        }

        if (ctx_->fmtCtx_->duration != AV_NOPTS_VALUE) {
            double dur = (double)ctx_->fmtCtx_->duration / AV_TIME_BASE;
            if (pos > dur) {
                pos = dur;
            }
        }

        pos *= AV_TIME_BASE;
        incr *= AV_TIME_BASE;
    }
    if (!ctx_->seekReq_) {
        ctx_->seekPos_ = pos;
        ctx_->seekRel_ = incr;
        if (seekByBytes && !(ctx_->fmtCtx_->iformat->flags & AVFMT_NO_BYTE_SEEK)) {
            ctx_->seekFlags_ |= AVSEEK_FLAG_BYTE;
        }
        else {
            ctx_->seekFlags_ &= ~AVSEEK_FLAG_BYTE;
        }
        ctx_->seekReq_ = 1;
        do {
            double pos_sec = (double)ctx_->seekPos_ / AV_TIME_BASE;
            double rel_sec = (double)ctx_->seekRel_ / AV_TIME_BASE;
            int pos_h = (int)(pos_sec / 3600);
            int pos_m = (int)((pos_sec - pos_h * 3600) / 60);
            int pos_s = (int)(pos_sec - pos_h * 3600 - pos_m * 60);
            int rel_h = (int)(rel_sec / 3600);
            int rel_m = (int)((rel_sec - rel_h * 3600) / 60);
            int rel_s = (int)(rel_sec - rel_h * 3600 - rel_m * 60);
            av_log(nullptr, AV_LOG_INFO, "seek: pos=%02d:%02d:%02d, rel=%02d:%02d:%02d\n",
                pos_h, pos_m, pos_s, rel_h, rel_m, rel_s);
        } while (0);
        ctx_->demuxCond_.notify_one();
    }
}

void Demuxer::Run() {
    DemuxLoop();
}

void Demuxer::DemuxLoop() {
    int ret = 0;
    bool lastPaused = false;
    AVPacket* pkt = av_packet_alloc();
    for (;;) {
        if (stop_) {
			av_log(nullptr, AV_LOG_INFO, "request quit while demux loop\n");
            break;
        }

        if (!ctx_->fmtCtx_) {
            av_log(nullptr, AV_LOG_WARNING, "fmt_ctx is null in demux loop, exiting\n");
            break;
        }

        if (ctx_->seekReq_) {

            int64_t seekTarget = ctx_->seekPos_;
            int64_t seekMin = ctx_->seekRel_ > 0 ? seekTarget - ctx_->seekRel_ + 2 : INT64_MIN;
            int64_t seekMax = ctx_->seekRel_ < 0 ? seekTarget - ctx_->seekRel_ - 2 : INT64_MAX;
            ret = avformat_seek_file(ctx_->fmtCtx_, -1, seekMin, seekTarget, seekMax, ctx_->seekFlags_);
            if (ret < 0) {
				av_log(nullptr, AV_LOG_WARNING, "avformat_seek_file failed, ret=%d\n", ret);
            }
            else {
				// flush packet queue
                if (ctx_->audioIndex_ >= 0) {
                    ctx_->audioPacketQueue_.Flush();
                }
                if (ctx_->videoIndex_ >= 0) {
                    ctx_->videoPacketQueue_.Flush();
                }
                if (ctx_->subtitleIndex_ >= 0) {
                    ctx_->subtitlePacketQueue_.Flush();
                }
                if (ctx_->dataIndex_ >= 0) {
                    ctx_->dataPacketQueue_.Flush();
                }
                // TODO
                // if (m_ctx->seek_flags & AVSEEK_FLAG_BYTE) {
                //     m_ctx->extern_clock.set(NAN, 0);
                // } else {
                //     m_ctx->extern_clock.set(seek_target / (double)AV_TIME_BASE, 0);
                // }
            }
            ctx_->seekReq_ = false;
            // m_ctx->eof = 0;
            // if (is->paused) {
            //     step_to_next_frame();
            // }
        }

        if (ctx_->paused_ != lastPaused) {
            lastPaused = ctx_->paused_;
            if (ctx_->paused_) {
                av_read_pause(ctx_->fmtCtx_);
            }
            else {
                av_read_play(ctx_->fmtCtx_);
            }
        }

        // TODO queue_attachments_req

        if (ctx_->audioPacketQueue_.Size() +
            ctx_->videoPacketQueue_.Size() +
            ctx_->subtitlePacketQueue_.Size() +
            ctx_->dataPacketQueue_.Size() > MAX_QUEUE_SIZE ||
            ctx_->audioPacketQueue_.Count() > MIN_FRAMES ||
            ctx_->videoPacketQueue_.Count() > MIN_FRAMES ||
            ctx_->subtitlePacketQueue_.Count() > MIN_FRAMES ||
            ctx_->dataPacketQueue_.Count() > MIN_FRAMES) {
            std::unique_lock<std::mutex> lock(ctx_->demuxMutex_);
            ctx_->demuxCond_.wait_for(lock, std::chrono::milliseconds(10));
            continue;
        }

        ret = av_read_frame(ctx_->fmtCtx_, pkt);
        if (ret < 0) {
            if ((ret == AVERROR_EOF || avio_feof(ctx_->fmtCtx_->pb)) && !ctx_->eof_) {
                if (ctx_->videoIndex_ >= 0) {
					ctx_->videoPacketQueue_.PutFlushPacket(ctx_->videoIndex_);
                }
                if (ctx_->audioIndex_ >= 0) {
					ctx_->audioPacketQueue_.PutFlushPacket(ctx_->audioIndex_);
                }
                if (ctx_->subtitleIndex_ >= 0) {
					ctx_->subtitlePacketQueue_.PutFlushPacket(ctx_->subtitleIndex_);
                }
                if (ctx_->dataIndex_ >= 0) {
                    ctx_->dataPacketQueue_.PutFlushPacket(ctx_->dataIndex_);
                }
                ctx_->eof_ = 1;

            }

            std::unique_lock<std::mutex> lock{ ctx_->demuxMutex_ };
            ctx_->demuxCond_.wait_for(lock, std::chrono::milliseconds(10));
            continue;
        }
        else {
            ctx_->eof_ = 0;
        }

        if (pkt->stream_index == ctx_->audioIndex_) {
            ctx_->audioPacketQueue_.Put(pkt);
        }
        else if (pkt->stream_index == ctx_->videoIndex_) {
            ctx_->videoPacketQueue_.Put(pkt);
        }
        else if (pkt->stream_index == ctx_->subtitleIndex_) {
            ctx_->subtitlePacketQueue_.Put(pkt);
        }
        else if (pkt->stream_index == ctx_->dataIndex_) {
            ctx_->dataPacketQueue_.Put(pkt);
        }
        else {
            av_packet_unref(pkt);
        }
    }
    av_packet_free(&pkt);
}