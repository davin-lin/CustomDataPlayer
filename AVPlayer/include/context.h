#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>

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
    friend class DataPlayer;
public:
    Context(const char* filename) {
        this->filename_ = av_strdup(filename);
    }
    ~Context() {

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

        if (filename_) {
            av_free((void*)filename_);
        }
    }

private:
    const char* filename_ = nullptr;

    AVFormatContext* fmtCtx_ = nullptr; 

    int             audioIndex_ = -1;
    AVCodecContext* audioCodecCtx_ = nullptr; 
    AVStream* audioStream_ = nullptr;    
    PacketQueue     audioPacketQueue_;       
    Clock           audioClock_{ &audioPacketQueue_.serial_, SYNC_TYPE_AUDIO }; 
    FrameQueue      audioFrameQueue_{ &audioPacketQueue_ ,AUDIO_FRAME_QUEUE_SIZE, 1 }; 
    SwrContext* audioSwrCtx_ = nullptr;

    int             videoIndex_ = -1; 
    AVCodecContext* videoCodecCtx_ = nullptr;
    AVStream* videoStream_ = nullptr;
    PacketQueue     videoPacketQueue_;
    Clock           videoClock_{ &videoPacketQueue_.serial_, SYNC_TYPE_VIDEO };
    AVRational      videoFrameRate_;
    FrameQueue      videoFrameQueue_{ &videoPacketQueue_, VIDEO_FRAME_QUEUE_SIZE, 1 };
    double          videoFrameTimer_ = 0.0; 

    int             subtitleIndex_ = -1; 
    AVCodecContext* subtitleCodecCtx_ = nullptr; 
    AVStream* subtitleStream_ = nullptr;
    PacketQueue     subtitlePacketQueue_; 
    FrameQueue      subtitleFrameQueue_{ &subtitlePacketQueue_ , SUBTITLE_FRAME_QUEUE_SIZE, 1 };

    int             dataIndex_ = -1;
    AVStream*       dataStream_ = nullptr;
    PacketQueue     dataPacketQueue_; 

    std::vector<std::string> streamOverlayLines_;
    std::mutex streamOverlayMutex_;

    // TODO
    // Clock           extern_clock{&extern_clock.m_serial, SYNC_TYPE_EXTERN}; 

    double maxFrameDuration_ = 0.0;

    std::mutex pauseMutex_;
    std::condition_variable pauseCond_;
    std::atomic<bool> paused_ = false;
    std::atomic<bool> stop_ = false;

    std::atomic<bool> seekReq_ = false;
    int seekFlags_ = 0;
    int64_t seekPos_ = 0;
    int64_t seekRel_ = 0;

    int eof_ = 0;

    int forceRefresh_ = 0;


    Clock* masterClock_ = &audioClock_;

    double frameLastReturnedTime_ = 0.0; 
    double frameLastFilterDelay_ = 0.0;  
    int frameDropsEarly_ = 0;
    int frameDropsLate_ = 0;


    AVInputFormat* iformat_ = nullptr;

    std::mutex demuxMutex_;
    std::condition_variable demuxCond_;
};