#include "frame_queue.h"

FrameQueue::FrameQueue(PacketQueue* pktq, int maxSize, int keepLast)
    : pktq_(pktq),
    keepLast_(!!keepLast) {
    maxSize_ = std::min(maxSize, FRAME_QUEUE_SIZE);
    for (size_t i = 0; i < maxSize_; i++) {
        queue_[i].frame_ = av_frame_alloc();
        queue_[i].Reset();
    }
}
FrameQueue::~FrameQueue() {
    for (size_t i = 0; i < maxSize_; i++) {
        Frame* vp = &queue_[i];
        queue_[i].Reset();
        av_frame_free(&queue_[i].frame_);
    }
}

int FrameQueue::Nb_Remaining() {
    return size_ - rindexShown_;
}

int FrameQueue::RindexShown() {
    return rindexShown_;
}

int64_t FrameQueue::LastPos() {
    Frame* fp = &queue_[rindex_];
    if (rindexShown_ && fp->serial_ == pktq_->Serial()) {
        return fp->pos_;
    }
    return -1;
}


Frame* FrameQueue::PeekWritable() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (size_ >= maxSize_ && !pktq_->RequestAborted() && !abort_) {
        cond_.wait(lock);
    }
    if (pktq_->RequestAborted() || abort_) {
        return nullptr;
    }
    return &queue_[windex_];
}


void FrameQueue::Push() {
    if (++windex_ == maxSize_) {
        windex_ = 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    size_++;
    cond_.notify_one();
}


Frame* FrameQueue::PeekReadable() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (size_ - rindexShown_ <= 0 && !pktq_->RequestAborted() && !abort_) {
        cond_.wait(lock);
    }
    if (pktq_->RequestAborted() || abort_) {
        return nullptr;
    }
    return &queue_[(rindex_ + rindexShown_) % maxSize_];
}


void FrameQueue::Next() {
    if (keepLast_ && !rindexShown_) {
        rindexShown_ = 1;
        return;
    }
    queue_[rindex_].Reset();
    if (++rindex_ == maxSize_) {
        rindex_ = 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    size_--;
    cond_.notify_one();
}

Frame* FrameQueue::PeekLast() {
    return &queue_[rindex_];
}

Frame* FrameQueue::Peek() {
    return &queue_[(rindex_ + rindexShown_) % maxSize_];
}

Frame* FrameQueue::PeekNext() {
    return &queue_[(rindex_ + rindexShown_ + 1) % maxSize_];
}

void FrameQueue::Lock() {
    mutex_.lock();
}
void FrameQueue::Unlock() {
    mutex_.unlock();
}

void FrameQueue::Wakeup() {
    std::lock_guard<std::mutex> lock(mutex_);
    cond_.notify_one();
}

void FrameQueue::Abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    abort_ = true;
    cond_.notify_one();
}