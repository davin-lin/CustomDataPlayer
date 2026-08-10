#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "opts.h"
#include "packet_queue.h"
#include "frame.h"

class FrameQueue {
public:
    FrameQueue(PacketQueue* pktq, int maxSize, int keepLast);
    ~FrameQueue();
    Frame* PeekWritable();
    void Push();
    Frame* PeekReadable();
    void Next();

    Frame* PeekLast();
    Frame* Peek();
    Frame* PeekNext();

    int Nb_Remaining();
    int RindexShown();
    int64_t LastPos();

    void Lock();
    void Unlock();
    void Wakeup();
    void Abort();
private:
    Frame queue_[FRAME_QUEUE_SIZE] = { 0 };
    PacketQueue* pktq_ = nullptr; 
    int maxSize_ = FRAME_QUEUE_SIZE;
    int keepLast_ = 0;    
    int rindexShown_ = 0; 
    int size_ = 0;         
    int rindex_ = 0;       
    int windex_ = 0;       

    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> abort_{ false };
};