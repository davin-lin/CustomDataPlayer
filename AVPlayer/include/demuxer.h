#pragma once

#include <memory>
#include "thread_base.h"
#include "context.h"

class Demuxer : public ThreadBase {
public:
    Demuxer(std::shared_ptr<Context> ctx);
    ~Demuxer();
    int Open();
    int Close();
    void Seek(double incr, int seekByBytes);
    virtual void Run() override;
private:
    void DemuxLoop();
private:
    std::shared_ptr<Context> ctx_;
};