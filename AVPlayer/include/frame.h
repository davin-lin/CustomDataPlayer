#pragma once

#include <mutex>
#include <condition_variable>
#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/frame.h>

#ifdef __cplusplus
}
#endif
#include "opts.h"
#include "packet_queue.h"

class Frame {
public:
    double VfDuration(Frame* nextvp, double maxFrameDuration);
    void Reset();

    AVFrame* frame_ = nullptr; 
    int serial_ = 0; 
    double pts_ = 0.0; 
    double duration_ = 0.0; 
    int uploaded_ = 0; 
    int64_t pos_ = 0;
    int width_ = 0;
    int height_ = 0; /* byte position of the frame in the input file */
};