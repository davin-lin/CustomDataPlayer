#pragma once

#include <string>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

struct CustomData {
    bool hasData = false;
    std::string usrName;
    std::string usrCompany;
    std::string usrType;
};

CustomData ParseCustomDataFromJson(AVFormatContext* fmtCtx);
