#include "custom_data.h"
#include "json.hpp"

using json = nlohmann::json;

CustomData ParseCustomDataFromJson(AVFormatContext* fmtCtx) {
    CustomData result;
    if (!fmtCtx) return result;

    AVDictionaryEntry* entry = av_dict_get(fmtCtx->metadata, "video_custom_data", nullptr, 0);
    if (!entry || !entry->value) {
        av_log(nullptr, AV_LOG_INFO, "No video_custom_data metadata found\n");
        return result;
    }

    try {
        json j = json::parse(entry->value);
        result.usrName    = j.value("usr_name", "");
        result.usrCompany = j.value("usr_company", "");
        result.usrType    = j.value("usr_type", "");
        result.hasData = true;
        av_log(nullptr, AV_LOG_INFO,
            "Custom metadata: name=%s company=%s type=%s\n",
            result.usrName.c_str(), result.usrCompany.c_str(), result.usrType.c_str());
    } catch (const std::exception& e) {
        av_log(nullptr, AV_LOG_WARNING, "Parse custom metadata failed: %s\n", e.what());
    }

    return result;
}
