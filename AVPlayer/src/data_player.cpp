#include "data_player.h"
#include "../../CustomMetadata/json.hpp"

using json = nlohmann::json;

DataPlayer::DataPlayer(std::shared_ptr<Context> ctx)
    : ctx_(ctx) {
}

DataPlayer::~DataPlayer() {
    Close();
    if (pkt_) {
        av_packet_free(&pkt_);
        pkt_ = nullptr;
    }
}

int DataPlayer::Open() {
    if (!ctx_->fmtCtx_) {
        av_log(nullptr, AV_LOG_ERROR, "[DataPlayer] fmtCtx is null, cannot open\n");
        return -1;
    }

    for (unsigned int i = 0; i < ctx_->fmtCtx_->nb_streams; ++i) {
        AVStream* s = ctx_->fmtCtx_->streams[i];
        if (s->codecpar->codec_type == AVMEDIA_TYPE_DATA &&
            s->codecpar->codec_id == AV_CODEC_ID_BIN_DATA) {
            ctx_->dataIndex_ = static_cast<int>(i);
            ctx_->dataStream_ = s;
            break;
        }
    }

    // 无 DATA 流时直接返回,Start/Close 内部会据此跳过
    if (ctx_->dataIndex_ < 0) {
        av_log(nullptr, AV_LOG_INFO, "[DataPlayer] no DATA stream, skip open\n");
        return -1;
    }
    pkt_ = av_packet_alloc();
    if (!pkt_) {
        av_log(nullptr, AV_LOG_ERROR, "[DataPlayer] av_packet_alloc failed\n");
        return -1;
    }
    av_log(nullptr, AV_LOG_INFO,
        "[DataPlayer] open ok, dataIndex=%d time_base=%d/%d\n",
        ctx_->dataIndex_,
        ctx_->dataStream_->time_base.num,
        ctx_->dataStream_->time_base.den);
    return 0;
}

int DataPlayer::Start() {
    if (ctx_->dataIndex_ < 0) {
        return 0;
    }
    ThreadBase::Start();
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] started\n");
    return 0;
}

int DataPlayer::Close() {
    if (ctx_->dataIndex_ >= 0) {
        // 中止队列,解除 Get 的阻塞等待
        ctx_->dataPacketQueue_.AbortRequest();
    }
    Stop();
    av_log(nullptr, AV_LOG_INFO,
        "[DataPlayer] closed, total received=%lld\n", (long long)recvCount_);
    return 0;
}

void DataPlayer::Run() {
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] run loop enter\n");
    while (!stop_) {
        // 队列被 abort(Get 返回 -1)或收到退出请求时退出循环
        if (GetDataPacket() < 0) {
            break;
        }
    }
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] run loop exit\n");
}

int DataPlayer::GetDataPacket() {
    int serial = -1;
    // block=1: 队列为空时阻塞等待,与 decoder 取包方式一致
    if (ctx_->dataPacketQueue_.Get(pkt_, 1, serial) < 0) {
        return -1;
    }
    pktSerial_ = serial;

    // flush/EOF 包:data==nullptr (由 demuxer 在 EOF 时 PutFlushPacket 投递)
    if (!pkt_->data) {
        av_log(nullptr, AV_LOG_INFO,
            "[DataPlayer] recv flush/EOF packet, serial=%d recv=%lld\n",
            serial, (long long)recvCount_);
        av_packet_unref(pkt_);
        // EOF 后 demuxer 不再投递数据,保持线程存活直至 Close abort 队列
        return 0;
    }

    ++recvCount_;

    // 消费后通知 demuxer,缓解队列背压(与 decoder 一致)
    if (ctx_->dataPacketQueue_.Count() == 0) {
        ctx_->demuxCond_.notify_one();
    }

    // 解析 JSON 字段(参考 CustomMetadata/json_stream_data.h):
    //   frame / pts / time_ms / data_id / value / speed /
    //   temperature / message / longitude / latitude
    std::string jsonString(reinterpret_cast<const char*>(pkt_->data), pkt_->size);

    // 仅打印前 20 条用于验证,避免日志刷屏
    if (logCount_ < 20) {
        try {
            json j = json::parse(jsonString);
            av_log(nullptr, AV_LOG_INFO,
                "[DataPlayer] #%lld | frame=%lld pts=%lld time_ms=%lld data_id=%lld "
                "value=%lld speed=%.3f temp=%.3f msg=%s lon=%.6f lat=%.6f "
                "(pkt pts=%lld size=%d serial=%d)\n",
                (long long)logCount_,
                (long long)j.value("frame", (int64_t)0),
                (long long)j.value("pts", (int64_t)0),
                (long long)j.value("time_ms", (int64_t)0),
                (long long)j.value("data_id", (int64_t)0),
                (long long)j.value("value", (int64_t)0),
                j.value("speed", 0.0),
                j.value("temperature", 0.0),
                j.value("message", std::string()).c_str(),
                j.value("longitude", 0.0),
                j.value("latitude", 0.0),
                (long long)pkt_->pts, pkt_->size, serial);
        } catch (const std::exception& e) {
            av_log(nullptr, AV_LOG_WARNING,
                "[DataPlayer] parse JSON failed: %s, raw=%s\n",
                e.what(), jsonString.c_str());
        }
        ++logCount_;
    }

    av_packet_unref(pkt_);
    return 0;
}
