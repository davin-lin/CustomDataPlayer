#pragma once

#include <memory>
#include "context.h"
#include "thread_base.h"

class DataPlayer : public ThreadBase {
public:
    DataPlayer(std::shared_ptr<Context> ctx);
    ~DataPlayer();

    int Open();
    int Start();
    int Close();

    virtual void Run() override;
private:
    int GetDataPacket();
    void WaitForClock(double dataTime, int serial);
private:
    std::shared_ptr<Context> ctx_;
    AVPacket* pkt_ = nullptr;
    int pktSerial_ = -1;
    int64_t recvCount_ = 0;
    int64_t logCount_ = 0;
};

