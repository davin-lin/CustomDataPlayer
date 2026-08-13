#pragma once

#include <memory>
#include "context.h"
#include "thread_base.h"

class Decoder : public ThreadBase {
public:
    Decoder(std::shared_ptr<Context> ctx, AVMediaType mediaType);
    virtual ~Decoder();
    int Open();
    int Close();
    int Decode(AVCodecContext* codecCtx, AVFrame* frame);
protected:
    virtual int ComponentOpen(int streamIndex, AVCodecContext* codecCtx);
    virtual int ComponentClose();
    virtual void Run() override {};
protected:
    std::shared_ptr<Context> ctx_ = nullptr;
    AVMediaType mediaType_ = AVMEDIA_TYPE_UNKNOWN;

    int finished_ = 0;
    int pktSerial_ = -1;
    int packetPending_ = 0;//处理 解码器内部缓冲区已满，packet 暂时发不进去 的情况
    int recoderPts_ = -1;

    AVPacket* pkt_ = nullptr;
    PacketQueue* queue_ = nullptr;

    int64_t startPts_ = AV_NOPTS_VALUE;
    AVRational startPtsTb_;
    int64_t nextPts_ = AV_NOPTS_VALUE;
    AVRational nextPtsTb_;
};