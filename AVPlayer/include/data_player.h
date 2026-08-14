#pragma once

#include <memory>
#include "context.h"
#include "thread_base.h"

// DataPlayer: 消费 demuxer 投递到 dataPacketQueue_ 的自定义 DATA 包。
// 每个 packet->data 即一条 JSON 字符串(字段定义参考 CustomMetadata/json_stream_data.h),
// 无需解码器,按 PTS 与视频帧同步。当前阶段仅验证接收是否正常,渲染部分暂未实现。
class DataPlayer : public ThreadBase {
public:
    DataPlayer(std::shared_ptr<Context> ctx);
    ~DataPlayer();

    int Open();
    int Start();
    int Close();

    virtual void Run() override;
private:
    int GetDataPacket();   // 从 dataPacketQueue_ 阻塞取包并解析 JSON,用于验证接收
private:
    std::shared_ptr<Context> ctx_;
    AVPacket* pkt_ = nullptr;
    int pktSerial_ = -1;     // 当前处理包的 serial(用于 seek/flush 一致性)
    int64_t recvCount_ = 0;  // 已接收的 DATA 包总数
    int64_t logCount_ = 0;   // 已打印日志的条数(限制前 20 条,避免刷屏)
};
