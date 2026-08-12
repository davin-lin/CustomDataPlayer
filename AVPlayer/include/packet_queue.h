#pragma once

#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

class PacketQueue {
    struct Packet {
        AVPacket* pkt; // packet 数据
        int serial;    // 当前时序
    };
    friend class Context;
public:
    PacketQueue() = default;
    ~PacketQueue() {
        Flush();
    }
    PacketQueue(const PacketQueue&) = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;

    int Put(AVPacket* pkt);
	int PutFlushPacket(int streamIndex);
    int Get(AVPacket* pkt, int block, int& serial);
    int Count() const; // 队列中packet的个数
    int Size() const;  // 队列中packet占用的内存大小
    void Flush();
    void Destroy();
    void AbortRequest();

    int  Serial();
    bool RequestAborted();
private:
    int PutPrivate(AVPacket* pkt);
private:
    std::queue<Packet> queue_;
    int size_ = 0;         // 当前队列包的总大小
    int64_t duration_ = 0; // 当前队列包的总播放时长

    int serial_ = 1;
    std::atomic<bool> abortRequest_ = false;

    std::mutex mutex_;
    std::condition_variable cond_;
};