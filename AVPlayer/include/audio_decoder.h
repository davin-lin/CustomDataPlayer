#pragma once

#include "decoder.h"

class AudioDecoder : public Decoder {
public:
    AudioDecoder(std::shared_ptr<Context> ctx);
protected:
    virtual int ComponentOpen(int streamIndex, AVCodecContext* codecCtx) override;
    virtual int ComponentClose() override;
    virtual void Run() override;

    void DecodeLoop();
private:
    bool EnqueueFrame(AVFrame* frame);
};