#include "packet_queue.h"

int PacketQueue::Put(AVPacket* pkt) {

    AVPacket* pkt1 = av_packet_alloc();
    if (!pkt1) {
        av_packet_unref(pkt);
        return -1;
    }

    av_packet_move_ref(pkt1, pkt);

    int ret = 0;
    {
        std::lock_guard<std::mutex> lock{ mutex_ };
        ret = PutPrivate(pkt1);
    }
    if (ret < 0) {
        av_packet_free(&pkt1);
    }
    return ret;
}

int PacketQueue::PutFlushPacket(int streamIndex) {
    AVPacket flush_pkt{};

    flush_pkt.data = nullptr;
    flush_pkt.size = 0;
    flush_pkt.stream_index = streamIndex;
	return Put(&flush_pkt);
}

int PacketQueue::PutPrivate(AVPacket* pkt) {
    if (abortRequest_) {
        return -1;
    }
    Packet packet;
    packet.pkt = pkt;
    packet.serial = serial_;
    size_ += packet.pkt->size + sizeof(packet);
    duration_ += packet.pkt->duration;
    queue_.push(packet);
    cond_.notify_one();
    return 0;
}

int PacketQueue::Get(AVPacket* pkt, int block, int& serial) {
    int ret = 0;
    Packet packet;
    std::unique_lock<std::mutex> lock{ mutex_ };
    for (;;) {
        if (abortRequest_ || (!block && queue_.empty())) {
            ret = -1;
            break;
        }
        while (!abortRequest_ && queue_.empty()) {
            cond_.wait(lock);
        }
        if (abortRequest_) {
            ret = -1;
            break;
        }
        packet = queue_.front();
        size_ -= packet.pkt->size + sizeof(packet);
        duration_ -= packet.pkt->duration;
        serial = packet.serial;
        av_packet_move_ref(pkt, packet.pkt);
        av_packet_free(&packet.pkt);
        queue_.pop();
        ret = 1;
        break;
    }
    return ret;
}

int PacketQueue::Count() const {
    return queue_.size();
}

int PacketQueue::Size() const {
    return size_;
}

void PacketQueue::Flush() {
    Packet packet;
    std::unique_lock<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        packet = queue_.front();
        av_packet_free(&packet.pkt);
        queue_.pop();
    }
    serial_++;
    size_ = 0;
    duration_ = 0;
}

void PacketQueue::Destroy() {
    Flush();
}

int PacketQueue::Serial() {
    return serial_;
}

bool PacketQueue::RequestAborted() {
    return abortRequest_;
}

void PacketQueue::AbortRequest() {
    std::lock_guard<std::mutex> lock{ mutex_ };
    abortRequest_ = true;
    cond_.notify_one();
}