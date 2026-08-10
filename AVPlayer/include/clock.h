#pragma once

typedef enum SyncType {
    SYNC_TYPE_AUDIO,
    SYNC_TYPE_VIDEO,
    SYNC_TYPE_EXTERN,
} SYNC_TYPE;

class Clock {
    friend class Context;
public:
    Clock(int* pktSerial, SYNC_TYPE syncType);
    void Set(double pts, int serial);
    void Set_at(double pts, int serial, double time);
    double Get();
    int Serial();
    SYNC_TYPE SyncType() const;

private:
    int serial_ = 0;            // 时钟依赖packet队列的序列号
    int paused_ = 0;            // 播放/暂停操作
    double pts_ = 0.0;          // 时钟基准
    double ptsDrift_ = 0.0;    // 时钟基准减去我们更新时钟的时间
    double speed_ = 1.0;        // 倍速播放
    double lastUpdated_ = 0.0; // 上次时钟更新的时间
    int* pktSerial_ = nullptr; // packet队列中的序列号
    SYNC_TYPE syncType_ = SYNC_TYPE_AUDIO; // 同步类型

};