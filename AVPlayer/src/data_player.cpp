#include "data_player.h"
#include "../../CustomMetadata/json.hpp"
#include <cstdio>
#include <cmath>
#include <vector>

extern "C" {
#include <libavutil/time.h>
}

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
        if (GetDataPacket() < 0) {
            break;
        }
    }
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] run loop exit\n");
}

int DataPlayer::GetDataPacket() {
    int serial = -1;
    if (ctx_->dataPacketQueue_.Get(pkt_, 1, serial) < 0) {
        return -1;
    }
    pktSerial_ = serial;

    if (!pkt_->data && !ctx_->eof_) {
        av_log(nullptr, AV_LOG_INFO,
            "[DataPlayer] recv flush/EOF packet, serial=%d recv=%lld\n",
            serial, (long long)recvCount_);
        {
            std::lock_guard<std::mutex> lock(ctx_->streamOverlayMutex_);
            ctx_->streamOverlayLines_.clear();
        }
        av_packet_unref(pkt_);
        return 0;
    }

    ++recvCount_;

    if (ctx_->dataPacketQueue_.Count() == 0) {
        ctx_->demuxCond_.notify_one();
    }

    std::string jsonString(reinterpret_cast<const char*>(pkt_->data), pkt_->size);

    try {
        json j = json::parse(jsonString);

        int64_t frame       = j.value("frame", (int64_t)0);
        int64_t timeMs      = j.value("time_ms", (int64_t)0);
        int64_t dataId      = j.value("data_id", (int64_t)0);
        int64_t value       = j.value("value", (int64_t)0);
        double   speed      = j.value("speed", 0.0);
        double   temper     = j.value("temperature", 0.0);
        std::string msg     = j.value("message", std::string());
        double   longitude  = j.value("longitude", 0.0);
        double   latitude   = j.value("latitude", 0.0);

        char buf[256];
        std::vector<std::string> lines;
        snprintf(buf, sizeof(buf), "ID: %lld",      (long long)dataId);  lines.emplace_back(buf);
        snprintf(buf, sizeof(buf), "Value: %lld",   (long long)value);   lines.emplace_back(buf);
        snprintf(buf, sizeof(buf), "Speed: %.2f",   speed);              lines.emplace_back(buf);
        snprintf(buf, sizeof(buf), "Temp: %.2f",    temper);             lines.emplace_back(buf);
        lines.emplace_back("Msg: " + msg);
        snprintf(buf, sizeof(buf), "Lon: %.6f",     longitude);          lines.emplace_back(buf);
        snprintf(buf, sizeof(buf), "Lat: %.6f",     latitude);           lines.emplace_back(buf);

        double dataTime = 0.0;
        if (timeMs > 0) {
            dataTime = timeMs / 1000.0;
        } else if (ctx_->dataStream_ && ctx_->dataStream_->time_base.den != 0 &&
                   pkt_->pts != AV_NOPTS_VALUE) {
            dataTime = pkt_->pts * av_q2d(ctx_->dataStream_->time_base);
        }

        if (serial != ctx_->dataPacketQueue_.Serial()) {
            av_packet_unref(pkt_);
            return 0;
        }

        WaitForClock(dataTime, serial);

        if (serial != ctx_->dataPacketQueue_.Serial()) {
            av_packet_unref(pkt_);
            return 0;
        }

        {
            std::lock_guard<std::mutex> lock(ctx_->streamOverlayMutex_);
            ctx_->streamOverlayLines_ = std::move(lines);
        }

    } catch (const std::exception& e) {
        av_log(nullptr, AV_LOG_WARNING,
            "[DataPlayer] parse JSON failed: %s, raw=%s\n",
            e.what(), jsonString.c_str());
    }

    av_packet_unref(pkt_);
    return 0;
}
void DataPlayer::WaitForClock(double dataTime, int serial) {
    while (!stop_) {
        if (serial != ctx_->dataPacketQueue_.Serial()) {
            return;
        }
        double clock = ctx_->videoClock_.Get();
        if (isnan(clock)) {
            av_usleep(10000);
            continue;
        }

        if (clock >= dataTime) {
            return;
        }

        double waitSec = dataTime - clock;
        int waitMs = static_cast<int>(waitSec * 1000.0) + 1;
        if (waitMs < 1) waitMs = 1;
        if (waitMs > 20) waitMs = 20;
        av_usleep(waitMs * 1000);
    }
}
