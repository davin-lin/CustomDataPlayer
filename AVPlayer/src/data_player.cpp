#include "data_player.h"

#include "../../CustomMetadata/json.hpp"
#include "../../CustomMetadata/data.pb.h"

#include <cctype>
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

using json = nlohmann::json;

namespace {

enum { kFormatUnknown = 0, kFormatJson = 1, kFormatProtobuf = 2 };

int DetectFormat(const uint8_t* data, int size) {
    if (!data || size <= 0) return kFormatUnknown;
    size_t i = 0;
    while (i < (size_t)size && std::isspace(data[i])) {
        ++i;
    }
    if (i >= (size_t)size) return kFormatUnknown;
    return data[i] == '{' ? kFormatJson : kFormatProtobuf;
}

void BuildLines(const std::string& name,
                int64_t dataId, int64_t value, double speed, double temper,
                const std::string& msg, double longitude, double latitude,
                int64_t timeMs, std::vector<std::string>& lines, double& dataTime,
                AVStream* dataStream, int64_t pts) {
    char buf[256];
    if (!name.empty()) {
        snprintf(buf, sizeof(buf), "Name: %s", name.c_str());  lines.emplace_back(buf);
    }
    snprintf(buf, sizeof(buf), "ID: %lld",     (long long)dataId);   lines.emplace_back(buf);
    snprintf(buf, sizeof(buf), "Value: %lld",  (long long)value);    lines.emplace_back(buf);
    snprintf(buf, sizeof(buf), "Speed: %.2f",   speed);              lines.emplace_back(buf);
    snprintf(buf, sizeof(buf), "Temp: %.2f",   temper);             lines.emplace_back(buf);
    lines.emplace_back("Msg: " + msg);
    snprintf(buf, sizeof(buf), "Lon: %.6f",    longitude);           lines.emplace_back(buf);
    snprintf(buf, sizeof(buf), "Lat: %.6f",    latitude);            lines.emplace_back(buf);

    if (timeMs > 0) {
        dataTime = timeMs / 1000.0;
    } else if (dataStream && dataStream->time_base.den != 0 && pts != AV_NOPTS_VALUE) {
        dataTime = pts * av_q2d(dataStream->time_base);
    }
}

bool ParseJsonPayload(const std::string& payload, AVStream* dataStream, int64_t pts,
                     std::vector<std::string>& lines, double& dataTime) {
    try {
        json j = json::parse(payload);
        int64_t timeMs    = j.value("time_ms", (int64_t)0);
        int64_t dataId    = j.value("data_id", (int64_t)0);
        int64_t value     = j.value("value", (int64_t)0);
        double  speed     = j.value("speed", 0.0);
        double  temper    = j.value("temperature", 0.0);
        std::string msg   = j.value("message", std::string());
        double  longitude = j.value("longitude", 0.0);
        double  latitude  = j.value("latitude", 0.0);
        BuildLines(std::string(), dataId, value, speed, temper, msg, longitude, latitude,
                   timeMs, lines, dataTime, dataStream, pts);
        return true;
    } catch (const std::exception& e) {
        av_log(nullptr, AV_LOG_WARNING, "[DataPlayer] parse JSON failed: %s, raw=%s\n", e.what(), payload.c_str());
        return false;
    }
}

bool ParseProtobufPayload(const std::string& payload, AVStream* dataStream, int64_t pts,
                          std::vector<std::string>& lines, double& dataTime) {
    customstream::FrameData fd;
    if (!fd.ParseFromString(payload)) {
        av_log(nullptr, AV_LOG_WARNING, "[DataPlayer] parse Protobuf failed, size=%d\n", (int)payload.size());
        return false;
    }
    BuildLines(fd.name(), fd.data_id(), fd.value(), fd.speed(), fd.temperature(),
               fd.message(), fd.longitude(), fd.latitude(),
               fd.time_ms(), lines, dataTime, dataStream, pts);
    return true;
}

}  // namespace

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
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] open ok, dataIndex=%d time_base=%d/%d, format=auto\n",
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
        ctx_->dataFrameQueue_.Abort();
    }
    Stop();
    detectedFormat_ = kFormatUnknown;
    av_log(nullptr, AV_LOG_INFO, "[DataPlayer] closed, total received=%lld\n", (long long)recvCount_);
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

    if (!pkt_->data || pkt_->size == 0) {
        av_log(nullptr, AV_LOG_INFO, "[DataPlayer] recv flush/EOF packet, serial=%d recv=%lld\n",serial, (long long)recvCount_);
        av_packet_unref(pkt_);
        return 0;
    }

    ++recvCount_;

    if (ctx_->dataPacketQueue_.Count() == 0) {
        ctx_->demuxCond_.notify_one();
    }

    std::string payload(reinterpret_cast<const char*>(pkt_->data), pkt_->size);

    double dataTime = 0.0;
    std::vector<std::string> lines;

    if (detectedFormat_ == kFormatUnknown) {
        detectedFormat_ = DetectFormat(pkt_->data, pkt_->size);

        av_log(nullptr, AV_LOG_INFO, "[DataPlayer] detected format=%d (1=JSON, 2=Protobuf)\n", detectedFormat_);
    }

    bool ok = false;
    if (detectedFormat_ == kFormatJson) {
        ok = ParseJsonPayload(payload, ctx_->dataStream_, pkt_->pts, lines, dataTime);
        if (!ok) ok = ParseProtobufPayload(payload, ctx_->dataStream_, pkt_->pts, lines, dataTime);
    } else if (detectedFormat_ == kFormatProtobuf) {
        ok = ParseProtobufPayload(payload, ctx_->dataStream_, pkt_->pts, lines, dataTime);
        if (!ok) ok = ParseJsonPayload(payload, ctx_->dataStream_, pkt_->pts, lines, dataTime);
    }

    if (!ok) {
        av_log(nullptr, AV_LOG_WARNING, "[DataPlayer] parse failed for both formats, size=%d\n", pkt_->size);
        av_packet_unref(pkt_);
        return 0;
    }

    Frame* sp = ctx_->dataFrameQueue_.PeekWritable();
    if (!sp) {
        av_packet_unref(pkt_);
        return 0;
    }

    sp->pts_ = dataTime;
    sp->serial_ = serial;
    sp->uploaded_ = 0;
    sp->dataLines_ = std::move(lines);

    ctx_->dataFrameQueue_.Push();

    av_packet_unref(pkt_);
    return 0;
}
