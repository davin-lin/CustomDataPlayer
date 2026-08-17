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

    if (!pkt_->data && !ctx_->eof_) {
        av_log(nullptr, AV_LOG_INFO,
            "[DataPlayer] recv flush/EOF packet, serial=%d recv=%lld\n",
            serial, (long long)recvCount_);
        // seek:清空叠加,避免残留旧数据
        {
            std::lock_guard<std::mutex> lock(ctx_->streamOverlayMutex_);
            ctx_->streamOverlayLines_.clear();
        }
        av_packet_unref(pkt_);
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

        // ====== 同步:按视频时钟节拍发布 ======
        // 显示时刻:优先用 JSON time_ms/1000(=视频帧 pts*视频时间基,与 videoClock_ 同域,单位秒);
        // 退化为 pkt->pts * DATA 时间基(=帧号/fps)
        double dataTime = 0.0;
        if (timeMs > 0) {
            dataTime = timeMs / 1000.0;
        } else if (ctx_->dataStream_ && ctx_->dataStream_->time_base.den != 0 &&
                   pkt_->pts != AV_NOPTS_VALUE) {
            dataTime = pkt_->pts * av_q2d(ctx_->dataStream_->time_base);
        }

        // seek 后旧包(serial 失效):直接丢弃,不等待不发布
        if (serial != ctx_->dataPacketQueue_.Serial()) {
            av_packet_unref(pkt_);
            return 0;
        }

        // 等待主时钟到达 dataTime(暂停时时钟停顿,自然等待;stop_/seek 可中断)
        WaitForClock(dataTime, serial);

        // 等待期间可能发生 seek -> 再次校验 serial,失效则丢弃,不发布过期数据
        if (serial != ctx_->dataPacketQueue_.Serial()) {
            av_packet_unref(pkt_);
            return 0;
        }

        // 时刻已到,发布到叠加缓冲(VideoPlayer 渲染时读取)
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
        // seek 发生:serial 失效,停止等待(调用方丢弃该包)
        if (serial != ctx_->dataPacketQueue_.Serial()) {
            return;
        }
        // 用视频时钟(=已显示帧的 pts)节拍,使叠加跟随显示帧,消除 A/V 偏差
        double clock = ctx_->videoClock_.Get();
        if (isnan(clock)) {
            // 时钟未就绪(刚启动/刚 seek,首帧尚未显示):等待,不发布。
            // 避免一次性把 demuxer 预读的大量包全部发布导致数字狂跳。
            av_usleep(10000);
            continue;
        }
        // 时刻已到或已过:发布
        if (clock >= dataTime) {
            return;
        }
        // 未到:小睡,上限 20ms 以保证 stop_/seek 响应及时
        double waitSec = dataTime - clock;
        int waitMs = static_cast<int>(waitSec * 1000.0) + 1;
        if (waitMs < 1) waitMs = 1;
        if (waitMs > 20) waitMs = 20;
        av_usleep(waitMs * 1000);
    }
}
