#pragma once

#include "decoder.h"
class VideoDecoder : public Decoder {
public:
    VideoDecoder(std::shared_ptr<Context> ctx);
protected:
    virtual int ComponentOpen(int streamIndex, AVCodecContext* codeCtx) override;
    virtual void Run() override;

    void DecodeLoop();
private:
    bool DropFrame(AVFrame* frame);
    bool EnqueueFrame(AVFrame* frame);
};