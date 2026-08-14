#pragma once

#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

// 自定义数据格式切换宏:
//   1 = JSON     (metadata key: "video_custom_data",         目标文件 walking-dead-json.mp4)
//   2 = Protobuf (metadata key: "video_custom_pb_data",      目标文件 walking-dead-protobuf.mp4)
#ifndef CUSTOM_DATA_FORMAT
#define CUSTOM_DATA_FORMAT 1
#endif

#define CUSTOM_DATA_FORMAT_JSON     1
#define CUSTOM_DATA_FORMAT_PROTOBUF 2

struct CustomData {
    bool hasData = false;
    std::string usrName;
    std::string usrCompany;
    std::string usrType;
};

CustomData ParseCustomDataFromJson(AVFormatContext* fmtCtx);
CustomData ParseCustomDataFromProtobuf(AVFormatContext* fmtCtx);

#if CUSTOM_DATA_FORMAT == CUSTOM_DATA_FORMAT_JSON
inline CustomData ParseCustomData(AVFormatContext* fmtCtx) {
    return ParseCustomDataFromJson(fmtCtx);
}
#define DEFAULT_MEDIA_PATH "D:\\software\\VisualStudio\\code\\CustomData\\walking-dead-json.mp4"
#elif CUSTOM_DATA_FORMAT == CUSTOM_DATA_FORMAT_PROTOBUF
inline CustomData ParseCustomData(AVFormatContext* fmtCtx) {
    return ParseCustomDataFromProtobuf(fmtCtx);
}
#define DEFAULT_MEDIA_PATH "D:\\software\\VisualStudio\\code\\CustomData\\walking-dead-protobuf.mp4"
#else
#warning "Invalid CUSTOM_DATA_FORMAT value"
#endif
