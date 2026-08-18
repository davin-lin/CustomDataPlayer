#include "audio_decoder.h"

AudioDecoder::AudioDecoder(std::shared_ptr<Context> ctx)
    : Decoder(ctx, AVMEDIA_TYPE_AUDIO) {
}

int AudioDecoder::ComponentOpen(int streamIndex, AVCodecContext* codecCtx) {
    ctx_->audioIndex_ = streamIndex;
    ctx_->audioCodecCtx_ = codecCtx;
    ctx_->audioStream_ = ctx_->fmtCtx_->streams[streamIndex];
    ctx_->audioCodecCtx_->pkt_timebase = ctx_->audioStream_->time_base;
    startPts_ = ctx_->audioStream_->start_time;
    startPtsTb_ = ctx_->audioStream_->time_base;
    queue_ = &ctx_->audioPacketQueue_;
    return 0;
}

void AudioDecoder::Run() {
    DecodeLoop();
}

void AudioDecoder::DecodeLoop() {
    int gotFrame = 0;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        av_log(nullptr, AV_LOG_ERROR, "av_frame_alloc failed\n");
        return;
    }
    while (!stop_) {
        gotFrame = Decode(ctx_->audioCodecCtx_, frame);

        if (gotFrame < 0) {
            break;
        }
        if (ctx_->audioPacketQueue_.Serial() != pktSerial_) {
            continue;
        }
        if (!EnqueueFrame(frame)) {
            continue;
        }
    };

    av_frame_free(&frame);
}

bool AudioDecoder::EnqueueFrame(AVFrame* frame) {
    //av_log(nullptr,
    //    AV_LOG_INFO,
    //    "enqueue audio frame samples=%d channels=%d format=%d pts=%lld\n",
    //    frame->nb_samples,
    //    frame->ch_layout.nb_channels,
    //    frame->format,
    //    frame->pts);
    Frame* af = ctx_->audioFrameQueue_.PeekWritable();
    if (!af) {
        return false;
    }
    af->pts_ = frame->pts == AV_NOPTS_VALUE ? NAN : av_q2d(ctx_->audioCodecCtx_->time_base) * frame->pts;
    af->pos_ = frame->pkt_pos;
    af->serial_ = pktSerial_;
    af->duration_ = av_q2d(AVRational{ frame->nb_samples, frame->sample_rate });

    av_frame_move_ref(af->frame_, frame);
    ctx_->audioFrameQueue_.Push();
    return true;
}