#pragma once
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include "json.hpp"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

using json = nlohmann::json;

// file path
const std::string g_srcPath = "D:\\software\\VisualStudio\\code\\CustomData\\walking-dead.mp4";
const std::string g_dstPath = "D:\\software\\VisualStudio\\code\\CustomData\\walking-dead-json.mp4";

static int CopyMuxWithMetadata() {
    int ret = 0;
    AVFormatContext* in_fmt = nullptr, * out_fmt = nullptr;
    AVPacket* pkt = nullptr;
    AVDictionary* mux_dict = nullptr; // muxer parameters: movflags=use_metadata_tags
    std::string str;
    json jObject;
    // ==================== fully consistent with command-line arguments ====================
    // 1. use_metadata_tags: Allow writing custom tags to MP4
    av_dict_set(&mux_dict, "movflags", "use_metadata_tags", 0);
    // 2. fflags + genpts: Automatically generate PTS timestamps (critical function; missing PTS will cause black screen and decoding failure)
    av_dict_set(&mux_dict, "fflags", "+genpts", AV_DICT_APPEND);
    // =================================================================

    // open input file
    ret = avformat_open_input(&in_fmt, g_srcPath.c_str(), nullptr, nullptr);
    if (ret < 0) goto cleanup;
    ret = avformat_find_stream_info(in_fmt, nullptr);
    if (ret < 0) goto cleanup;

    // Create output MP4 container
    ret = avformat_alloc_output_context2(&out_fmt, nullptr, "mp4", g_dstPath.c_str());
    if (ret < 0) goto cleanup;

    // Traverse all streams
    for (unsigned int i = 0; i < in_fmt->nb_streams; i++)
    {
        AVStream* in_stream = in_fmt->streams[i];
        AVStream* out_stream = avformat_new_stream(out_fmt, nullptr);
        if (!out_stream) { ret = AVERROR(ENOMEM); goto cleanup; }

        // Copy codec parameters (copy all SPS, PPS and extradata)
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) goto cleanup;

        out_stream->time_base = in_stream->time_base;
        out_stream->codecpar->codec_tag = 0; // Set stream copy flag, let the muxer automatically adapt to MP4 AVC format
    }

    jObject["usr_name"] = "linmingyang";
	jObject["usr_company"] = "OBSBOT";
    jObject["usr_type"] = "JSON";

    str = jObject.dump(4);

    // Save to global metadata dict
    av_dict_set(&out_fmt->metadata, "video_custom_data", str.c_str(), 0);
    // ====================================================================

    // open outPut file IO
    if (!(out_fmt->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&out_fmt->pb, g_dstPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) goto cleanup;
    }

    // Write MP4 header; mux_dict is need (required to enable movflags)
    ret = avformat_write_header(out_fmt, &mux_dict);
    if (ret < 0) goto cleanup;

    pkt = av_packet_alloc();

    while (av_read_frame(in_fmt, pkt) >= 0)
    {
        AVStream* in_stream = in_fmt->streams[pkt->stream_index];
        AVStream* out_stream = out_fmt->streams[pkt->stream_index];

        // transform PTS/DTS to the output stream's time_base
        av_packet_rescale_ts(pkt, in_stream->time_base, out_stream->time_base);
        pkt->stream_index = pkt->stream_index;

        ret = av_interleaved_write_frame(out_fmt, pkt);
        if (ret < 0) break;

        av_packet_unref(pkt);
    }

    // Write moov end index, complete encapsulation
    av_write_trailer(out_fmt);
    av_log(NULL, AV_LOG_INFO, "Encapsulation finished ,Output File: %s\n", g_dstPath.c_str());

cleanup:
    av_dict_free(&mux_dict);
    av_packet_free(&pkt);
    if (out_fmt)
    {
        if (!(out_fmt->oformat->flags & AVFMT_NOFILE))
            avio_closep(&out_fmt->pb);
        avformat_free_context(out_fmt);
    }
    avformat_close_input(&in_fmt);
    return ret < 0 ? ret : 0;
}

static int VerifyJsonMetadata() {
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, g_dstPath.c_str(), nullptr, nullptr);
    if (ret < 0)
    {
        std::cerr << "Open generated MP4 failed" << std::endl;
        return -1;
    }
    avformat_find_stream_info(fmt_ctx, nullptr);

    // Read custom tags
    AVDictionaryEntry* entry_pb = av_dict_get(fmt_ctx->metadata, "video_custom_data", nullptr, 0);
    if (!entry_pb)
    {
        std::cerr << "don't find metadata" << std::endl;
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    json j = json::parse(entry_pb->value);

    std::cout << "Read custom metadata:\n" << j << std::endl;
    avformat_close_input(&fmt_ctx);
    return 0;
}