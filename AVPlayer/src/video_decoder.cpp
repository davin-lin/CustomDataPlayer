#include "opts.h"
#include "video_decoder.h"

VideoDecoder::VideoDecoder(std::shared_ptr<Context> ctx)
    : Decoder(ctx, AVMEDIA_TYPE_VIDEO) {
}

int VideoDecoder::ComponentOpen(int streamIndex, AVCodecContext* codecCtx) {
    ctx_->videoIndex_ = streamIndex;
    ctx_->videoCodecCtx_ = codecCtx;
    ctx_->videoStream_ = ctx_->fmtCtx_->streams[streamIndex];
    ctx_->videoFrameRate_ = av_guess_frame_rate(ctx_->fmtCtx_, ctx_->videoStream_, nullptr);
    queue_ = &ctx_->videoPacketQueue_;
    return 0;
}

void VideoDecoder::Run() {
    DecodeLoop();
}

void VideoDecoder::DecodeLoop() {
    int got_frame = 0;
    AVFrame* frame = av_frame_alloc();
    while (!stop_) {
        got_frame = Decode(ctx_->videoCodecCtx_, frame);
        if (got_frame < 0) {
            break;
        }

        if (DropFrame(frame)) {
            continue;
        }
        // TODO add video filters, filter may add while 
        // TODO ���� frame_last_filter_delay

        // av_frame_unref(frame);
        if (ctx_->videoPacketQueue_.Serial() != pktSerial_) {
			av_log(nullptr, AV_LOG_WARNING, "the serial in video packet queue and decoder is different, serial=%d, serial=%d\n", 
                ctx_->videoPacketQueue_.Serial(), pktSerial_);
            continue;
        }

        if (!EnqueueFrame(frame)) {
            continue;
        }

    }
    av_frame_free(&frame);
    return;
}

bool VideoDecoder::DropFrame(AVFrame* frame) {
    double dpts = NAN;
    if (frame->pts != AV_NOPTS_VALUE) {
        dpts = av_q2d(ctx_->videoStream_->time_base) * frame->pts;
    }
    frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(ctx_->fmtCtx_, ctx_->videoStream_, frame);

    if ((g_framedrop > 0 || (g_framedrop && ctx_->masterClock_->SyncType() != SYNC_TYPE_VIDEO)) && frame->pts != AV_NOPTS_VALUE) {
        double diff = dpts - ctx_->masterClock_->Get();
        if (!isnan(diff) &&
            fabs(diff) < AV_NOSYNC_THRESHOLD &&
            diff - ctx_->frameLastFilterDelay_ < 0 &&
            pktSerial_ == ctx_->videoClock_.Serial() &&
            ctx_->videoPacketQueue_.Count())
        {
            ctx_->frameDropsEarly_++;
            av_frame_unref(frame); 
            return true;
        }
    }
    return false;
}

bool VideoDecoder::EnqueueFrame(AVFrame* frame) {
    Frame* vp = ctx_->videoFrameQueue_.PeekWritable();
    if (!vp) {
        return false;
    }

    AVRational tb = ctx_->videoStream_->time_base;
    AVRational frame_rate = av_guess_frame_rate(ctx_->fmtCtx_, ctx_->videoStream_, NULL);

    vp->uploaded_ = 0;
    vp->pts_ = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
    vp->duration_ = (frame_rate.num && frame_rate.den ? av_q2d(AVRational{ frame_rate.den, frame_rate.num }) : 0);
    vp->pos_ = frame->pkt_pos;
    vp->serial_ = pktSerial_;
    vp->width_ = frame->width;
    vp->height_ = frame->height;

    av_frame_move_ref(vp->frame_, frame);
    ctx_->videoFrameQueue_.Push();
    return true;
}