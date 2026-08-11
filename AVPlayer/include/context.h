#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

#ifdef __cplusplus
}
#endif

#include "clock.h"
#include "packet_queue.h"
#include "frame_queue.h"

#define MAX_QUEUE_SIZE (16*1024*1024)
#define MIN_FRAMES 10000

class Context {
    friend class Player;
    friend class Demuxer;
    friend class Decoder;
    friend class AudioDecoder;
    friend class VideoDecoder;
    friend class AudioPlayer;
    friend class VideoPlayer;
public:
    Context(const char* filename) {
        this->filename_ = av_strdup(filename);
    }
    ~Context() {
                // FFmpeg 资源（按依赖顺序释放）
        if (audioSwrCtx_) {
            swr_free(&audioSwrCtx_);
        }
        if (subtitleCodecCtx_) {
            avcodec_free_context(&subtitleCodecCtx_);
        }
        if (videoCodecCtx_) {
            avcodec_free_context(&videoCodecCtx_);
        }
        if (audioCodecCtx_) {
            avcodec_free_context(&audioCodecCtx_);
        }
        if (fmtCtx_) {
            avformat_close_input(&fmtCtx_);
        }
        if (filename_) {
            av_free((void*)filename_);
            filename_ = nullptr;
        }
        // PacketQueue / FrameQueue 有自己的析构函数，自动释放内部资源
        if (filename_) {
            av_free((void*)filename_);
        }
    }

private:
    const char* filename_ = nullptr; // 文件名

    AVFormatContext* fmtCtx_ = nullptr; // AVFormatContext 解封装上下文

    int             audioIndex_ = -1;          // 音频流索引
    AVCodecContext* audioCodecCtx_ = nullptr; // 音频解码器上下文
    AVStream* audioStream_ = nullptr;    // 音频流
    PacketQueue     audioPacketQueue_;        // 音频包队列
    Clock           audioClock_{ &audioPacketQueue_.serial_, SYNC_TYPE_AUDIO };               // 音频时钟
    FrameQueue      audioFrameQueue_{ &audioPacketQueue_ ,AUDIO_FRAME_QUEUE_SIZE, 1 }; // 音频帧队列
    SwrContext* audioSwrCtx_ = nullptr;

    int             videoIndex_ = -1;          // 视频流索引
    AVCodecContext* videoCodecCtx_ = nullptr; // 视频解码器上下文
    AVStream* videoStream_ = nullptr;    // 视频流
    PacketQueue     videoPacketQueue_;        // 视频包队列
    Clock           videoClock_{ &videoPacketQueue_.serial_, SYNC_TYPE_VIDEO };               // 视频时钟
    AVRational      videoFrameRate_;          // 视频帧率
    FrameQueue      videoFrameQueue_{ &videoPacketQueue_, VIDEO_FRAME_QUEUE_SIZE, 1 }; // 视频帧队列
    double          videoFrameTimer_ = 0.0; // 记录最后一帧视频播放的时刻

    int             subtitleIndex_ = -1;          // 字幕流索引
    AVCodecContext* subtitleCodecCtx_ = nullptr; // 字幕解码器上下文
    AVStream* subtitleStream_ = nullptr;    // 字幕流
    PacketQueue     subtitlePacketQueue_;        // 字幕包队列
    FrameQueue      subtitleFrameQueue_{ &subtitlePacketQueue_ , SUBTITLE_FRAME_QUEUE_SIZE, 1 }; // 字幕帧队列

    // TODO
    // Clock           extern_clock{&extern_clock.m_serial, SYNC_TYPE_EXTERN};                 // 外部时钟

    double maxFrameDuration_ = 0.0; // 一帧的最大间隔

    std::mutex pauseMutex_;
    std::condition_variable pauseCond_;
    std::atomic<bool> paused_ = false; // 暂停/播放
    std::atomic<bool> stop_ = false;   // 停止

    // seek请求
    std::atomic<bool> seekReq_ = false;    // seek请求
    int seekFlags_ = 0;
    int64_t seekPos_ = 0;
    int64_t seekRel_ = 0;

    int eof_ = 0;
    // 强制刷新标志
    int forceRefresh_ = 0;

    // 主时钟
    Clock* masterClock_ = &audioClock_;

    double frameLastReturnedTime_ = 0.0; // 用于记录上一帧在解码后被返回的时间戳
    double frameLastFilterDelay_ = 0.0;  // 用于记录上一帧通过滤镜链后的延迟时间
    int frameDropsEarly_ = 0; // 统计被丢弃的时钟有误差的包，放入frame队列之前丢弃
    int frameDropsLate_ = 0;


    AVInputFormat* iformat_ = nullptr;

    std::mutex demuxMutex_;
    std::condition_variable demuxCond_;

    // 自定义元数据(从 MP4 metadata 中的 "video_custom_data" JSON 解析)
    bool hasCustomData_ = false;
    std::string usrName_;
    std::string usrCompany_;
    std::string usrType_;
};