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
    void SetPaused(int p);
    double Get();
    int Serial();
    SYNC_TYPE SyncType() const;

private:
    int serial_ = 0;
    int paused_ = 0;
    double pts_ = 0.0; 
    double ptsDrift_ = 0.0;
    double speed_ = 1.0; 
    double lastUpdated_ = 0.0;
    int* pktSerial_ = nullptr;
    SYNC_TYPE syncType_ = SYNC_TYPE_AUDIO;

};