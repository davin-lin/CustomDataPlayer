#include "custom_data.h"
#include "json.hpp"
#include "usr.pb.h"

using json = nlohmann::json;

CustomData ParseCustomDataFromJson(AVFormatContext* fmtCtx) {
    CustomData result;
    if (!fmtCtx) return result;

    AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, "video_custom_data", nullptr, 0);
    if (!entry || !entry->value) {
        av_log(nullptr, AV_LOG_INFO, "[JSON] No video_custom_data metadata found\n");
        return result;
    }

    try {
        json j = json::parse(entry->value);
        result.usrName    = j.value("usr_name", "");
        result.usrCompany = j.value("usr_company", "");
        result.usrType    = j.value("usr_type", "");
        result.hasData = true;
        av_log(nullptr, AV_LOG_INFO,
            "[JSON] Custom metadata: name=%s company=%s type=%s\n",
            result.usrName.c_str(), result.usrCompany.c_str(), result.usrType.c_str());
    } catch (const std::exception& e) {
        av_log(nullptr, AV_LOG_WARNING, "[JSON] Parse custom metadata failed: %s\n", e.what());
    }

    return result;
}

CustomData ParseCustomDataFromProtobuf(AVFormatContext* fmtCtx) {
    CustomData result;
    if (!fmtCtx) return result;

    AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, "video_custom_pb_data", nullptr, 0);
    if (!entry || !entry->value) {
        av_log(nullptr, AV_LOG_INFO, "[Protobuf] No video_custom_pb_data metadata found\n");
        return result;
    }

    Usr usr;
    bool ok = usr.ParseFromString(std::string(entry->value));
    if (!ok) {
        av_log(nullptr, AV_LOG_WARNING, "[Protobuf] Parse custom metadata failed\n");
        return result;
    }

    result.usrName    = usr.name();
    result.usrCompany = usr.company();
    result.usrType    = usr.type();
    result.hasData = true;
    av_log(nullptr, AV_LOG_INFO,
        "[Protobuf] Custom metadata: name=%s company=%s type=%s\n",
        result.usrName.c_str(), result.usrCompany.c_str(), result.usrType.c_str());
    return result;
}
