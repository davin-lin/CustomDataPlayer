#include "clock.h"
#include "opts.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/mathematics.h>
#include <libavutil/time.h>

#ifdef __cplusplus
}
#endif

Clock::Clock(int* pktSerial, SYNC_TYPE syncType)
    : speed_(1.0),
    paused_(0),
    pktSerial_(pktSerial),
    syncType_(syncType) {
    Set(NAN, -1);
}

void Clock::Set(double pts, int serial) {
    double time = av_gettime_relative() / 1000000.0;
    Set_at(pts, serial, time);
}

void Clock::Set_at(double pts, int serial, double time) {
    pts_ = pts;
    lastUpdated_ = time;
    ptsDrift_ = pts_ - time;
    serial_ = serial;
}

void Clock::SetPaused(int p) {
    if (paused_ == p) {
        return;
    }
    paused_ = p;
    if (!p) {
        Set(pts_, serial_);
    }
}

double Clock::Get() {
    if (*pktSerial_ != serial_) {
        return NAN;
    }
    if (paused_) {
        return pts_;
    }
    else {
        double time = av_gettime_relative() / 1000000.0;
        return ptsDrift_ + time - (time - lastUpdated_) * (1.0 - speed_);
    }
}

int Clock::Serial() {
    return serial_;
}

SYNC_TYPE Clock::SyncType() const {
    return syncType_;
}

double Clock::LastUpdated() const {
    return lastUpdated_;
}