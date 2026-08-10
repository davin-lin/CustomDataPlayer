#include "frame.h"

double Frame::VfDuration(Frame* nextvp, double maxFrameDuration) {
    if (!nextvp) {
        return 0.0;
    }
    if (nextvp->serial_ != serial_) {
        return 0.0;
    }
    double duration = nextvp->pts_ - pts_;
    if (isnan(duration) || duration <= 0 || duration > maxFrameDuration) {
        return this->duration_;
    }
    return duration;
}

void Frame::Reset() {
    av_frame_unref(frame_);
    serial_ = 0;
    pts_ = 0.0;
    duration_ = 0.0;
    uploaded_ = 0;
    pos_ = 0;
}