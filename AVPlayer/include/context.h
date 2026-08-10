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
        if (filename_) {
            av_free((void*)filename_);
        }
    }

private:
    const char* filename_ = nullptr; // �ļ���

    AVFormatContext* fmtCtx_ = nullptr; // ���װ��������

    int             audioIndex_ = -1;          // ��Ƶ������
    AVCodecContext* audioCodecCtx_ = nullptr; // ��Ƶ��������������
    AVStream* audioStream_ = nullptr;    // ��Ƶ��
    PacketQueue     audioPacketQueue_;        // ��Ƶpacket����
    Clock           audioClock_{ &audioPacketQueue_.serial_, SYNC_TYPE_AUDIO };               // ��Ƶʱ��
    FrameQueue      audioFrameQueue_{ &audioPacketQueue_ ,AUDIO_FRAME_QUEUE_SIZE, 1 }; //��Ƶ֡����
    SwrContext* audioSwrCtx_ = nullptr;

    int             videoIndex_ = -1;          // ��Ƶ������
    AVCodecContext* videoCodecCtx_ = nullptr; // ��Ƶ��������������
    AVStream* videoStream_ = nullptr;    // ��Ƶ��
    PacketQueue     videoPacketQueue_;        // ��Ƶpacket����
    Clock           videoClock_{ &videoPacketQueue_.serial_, SYNC_TYPE_VIDEO };               // ��Ƶʱ��
    AVRational      videoFrameRate_;          // ��Ƶ֡��
    FrameQueue      videoFrameQueue_{ &videoPacketQueue_, VIDEO_FRAME_QUEUE_SIZE, 1 }; // ��Ƶ֡����
    double          videoFrameTimer_ = 0.0; // ��¼���һ֡��Ƶ���ŵ�ʱ��

    int             subtitleIndex_ = -1;          // ��Ļ������
    AVCodecContext* subtitleCodecCtx_ = nullptr; // ��Ļ��������������
    AVStream* subtitleStream_ = nullptr;    // ��Ļ��
    PacketQueue     subtitlePacketQueue_;        // ��Ļpacket����
    FrameQueue      subtitleFrameQueue_{ &subtitlePacketQueue_ , SUBTITLE_FRAME_QUEUE_SIZE, 1 }; // ��Ļ֡����

    // TODO
    // Clock           extern_clock{&extern_clock.m_serial, SYNC_TYPE_EXTERN};                 // �ⲿʱ��

    double maxFrameDuration_ = 0.0; // һ֡�������

    std::mutex pauseMutex_;
    std::condition_variable pauseCond_;
    std::atomic<bool> paused_ = false; // ��ͣ/�ָ�����
    std::atomic<bool> stop_ = false;   // ֹͣ����

    // seek����
    std::atomic<bool> seekReq_ = false;    // seek����
    int seekFlags_ = 0;
    int64_t seekPos_ = 0;
    int64_t seekRel_ = 0;

    int eof_ = 0;
    // ǿ��ˢ����Ƶ
    int forceRefresh_ = 0;

    // ����Ƶͬ��
    Clock* masterClock_ = &audioClock_;

    double frameLastReturnedTime_ = 0.0; // ���ڼ�¼��һ֡�ڽ���󱻷��ص�ʱ���
    double frameLastFilterDelay_ = 0.0;  // ���ڼ�¼��һ֡ͨ���˾�������ӳ�ʱ��
    int frameDropsEarly_ = 0; // ͳ�Ʊ�������ʱ�������İ�������frame����֮ǰ����
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