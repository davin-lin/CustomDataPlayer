#pragma once
#include <thread>
#include <atomic>

class ThreadBase {
public:
    ThreadBase();
    virtual ~ThreadBase();

    void Start();
    void Stop();
    virtual void Run() = 0;
protected:
    std::thread thread_;
    std::atomic<bool> stop_ = false;
};