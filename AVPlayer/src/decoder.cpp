#include "decoder.h"

Decoder::Decoder(std::shared_ptr<Context> ctx, AVMediaType mediaType)
    : ctx_(ctx),
    mediaType_(mediaType),
    pkt_(av_packet_alloc())
{
}

Decoder::~Decoder() {
    if (pkt_) {
        av_packet_free(&pkt_);
        pkt_ = nullptr;
    }
    Close();
}

int Decoder::Open() {
    const AVCodec* codec = nullptr;
    AVFormatContext* fmtCtx = ctx_->fmtCtx_;
    int streamIndex = av_find_best_stream(fmtCtx, mediaType_, -1, -1, &codec, 0);
    if (streamIndex < 0 || streamIndex >= (int)fmtCtx->nb_streams) {
		av_log(nullptr, AV_LOG_ERROR, "av_find_best_stream failed, stream_index=%d\n", streamIndex);
        return -1;
    }
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, fmtCtx->streams[streamIndex]->codecpar);

    // 视频解码启用多线程（帧级并行），音频不启用（收益太小）
    if (mediaType_ == AVMEDIA_TYPE_VIDEO) {
        // 最多使用 4 个线程，避免过度竞争
        codecCtx->thread_count = 4;
        codecCtx->thread_type  = FF_THREAD_FRAME;
    }

    int ret = avcodec_open2(codecCtx, codec, nullptr);
    if (ret < 0) {
		av_log(nullptr, AV_LOG_ERROR, "avcodec_open2 failed, ret=%d\n", ret);
        return -1;
    }
    return ComponentOpen(streamIndex, codecCtx);
}

int Decoder::Close() {
    if (queue_) {
        queue_->AbortRequest();
    }
    ctx_->audioFrameQueue_.Wakeup();
    ctx_->videoFrameQueue_.Wakeup();
    Stop();
    return ComponentClose();
}

int Decoder::Decode(AVCodecContext* codecCtx, AVFrame* frame) {
    int ret = AVERROR(EAGAIN);

    for (;;) {
        if (queue_->Serial() == pktSerial_) {
            do {
                if (queue_->RequestAborted()) {
					av_log(nullptr, AV_LOG_WARNING, "request aborted in decoder decode\n");
                    return -1;
                }
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret >= 0) {
                    if (AVMEDIA_TYPE_AUDIO == codecCtx->codec_type) {
                        AVRational tb = AVRational{ 1, frame->sample_rate };
                        if (frame->pts != AV_NOPTS_VALUE) {
                            frame->pts = av_rescale_q(frame->pts, codecCtx->pkt_timebase, tb);
                        }
                        else if (nextPts_ != AV_NOPTS_VALUE) {
                            frame->pts = av_rescale_q(nextPts_, nextPtsTb_, tb);
                        }
                        if (frame->pts != AV_NOPTS_VALUE) {
                            nextPts_ = frame->pts + frame->nb_samples;
                            nextPtsTb_ = tb;
                        }
                    }
                    else if (AVMEDIA_TYPE_VIDEO == codecCtx->codec_type) {
                        if (recoderPts_ == -1) {
                            frame->pts = frame->best_effort_timestamp;
                        }
                        else if (!recoderPts_) {
                            frame->pts = frame->pkt_dts;
                        }
                    }
                }

                if (AVERROR_EOF == ret) {
                    finished_ = pktSerial_;
                    avcodec_flush_buffers(codecCtx);
					av_log(nullptr, AV_LOG_INFO, "decoder decode got EOF\n");
                    return 0;
                }

                if (ret >= 0) {
					av_log(nullptr, AV_LOG_DEBUG, "decoder decode got frame, pts=%lld\n", frame->pts);
                    return 1;
                }
            } while (ret != AVERROR(EAGAIN));
        }

        do {
            if (queue_->Count() == 0) {
                ctx_->demuxCond_.notify_one();
            }
            if (packetPending_) {
                packetPending_ = 0;
            }
            else {
                int oldSerial = pktSerial_;
                if (queue_->Get(pkt_, 1, pktSerial_) < 0) {
					av_log(nullptr, AV_LOG_WARNING, "get packet from queue failed\n");
                    return -1;
                }
                if (oldSerial != pktSerial_) {
                    avcodec_flush_buffers(codecCtx);
                    finished_ = 0;
                    nextPts_ = startPts_;
                    nextPtsTb_ = startPtsTb_;
                }
            }
            if (queue_->Serial() == pktSerial_) {
                break;
            }
            av_packet_unref(pkt_);
        } while (1);

        // TODO subtitle

        if (avcodec_send_packet(codecCtx, pkt_) == AVERROR(EAGAIN)) {
            packetPending_ = 1;
        }
        else {
            av_packet_unref(pkt_);
        }

    }
}

int Decoder::ComponentOpen(int stream_index, AVCodecContext* codec_ctx) {
    return 0;
}

int Decoder::ComponentClose() {
    return 0;
}