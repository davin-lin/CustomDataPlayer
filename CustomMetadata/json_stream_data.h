#pragma once
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

#include "json.hpp"

using json = nlohmann::json;

static void PrintError(const char* msg, int ret)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, errbuf, sizeof(errbuf));
    std::cerr << msg << ": " << errbuf << std::endl;
}

static int CreateJsonStreamMP4()
{
    const char* inputFile = "D:\\tmp\\walking-dead.mp4";
    const char* outputFile = "D:\\tmp\\walking-dead-json-stream.mp4";
    const char* jsonFile = "D:\\tmp\\data.json";

    // 打开输入文件
    AVFormatContext* inputCtx = nullptr;
    int ret = avformat_open_input(&inputCtx, inputFile, nullptr, nullptr);

    if (ret < 0) {
        PrintError("avformat_open_input failed", ret);
        return -1;
    }

    ret = avformat_find_stream_info(inputCtx, nullptr);

    if (ret < 0) {
        PrintError("avformat_find_stream_info failed", ret);
        avformat_close_input(&inputCtx);
        return -1;
    }

    // 查找视频流
    int videoStreamIndex = -1;

    for (unsigned int i = 0; i < inputCtx->nb_streams; ++i) {
        AVStream* stream = inputCtx->streams[i];

        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0) {
        std::cerr << "No video stream found." << std::endl;
        avformat_close_input(&inputCtx);
        return -1;
    }

    AVStream* videoStream = inputCtx->streams[videoStreamIndex];

    // 获取 FPS
    AVRational frameRate = videoStream->avg_frame_rate;

    if (frameRate.num == 0 || frameRate.den == 0)
        frameRate = videoStream->r_frame_rate;

    double fps = av_q2d(frameRate);

    if (fps <= 0.0) {
        std::cerr << "Invalid FPS." << std::endl;
        avformat_close_input(&inputCtx);
        return -1;
    }

    // 获取视频时长
    double durationSeconds = 0.0;

    if (inputCtx->duration != AV_NOPTS_VALUE)
        durationSeconds = static_cast<double>(inputCtx->duration) / AV_TIME_BASE;

    int64_t frameCount = videoStream->nb_frames;

    std::cout << "========================================\n";
    std::cout << "Video Information\n";
    std::cout << "========================================\n";
    std::cout << "Width       : " << videoStream->codecpar->width << "\n";
    std::cout << "Height      : " << videoStream->codecpar->height << "\n";
    std::cout << "FPS         : " << fps << "\n";
    std::cout << "Duration    : " << durationSeconds << " seconds\n";
    std::cout << "Frame count : " << frameCount << "\n";
    std::cout << "Time base   : " << videoStream->time_base.num << "/" << videoStream->time_base.den << "\n";

    // 创建输出文件
    AVFormatContext* outputCtx = nullptr;
    ret = avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputFile);

    if (ret < 0 || !outputCtx) {
        PrintError("avformat_alloc_output_context2 failed", ret);
        avformat_close_input(&inputCtx);
        return -1;
    }

    // 复制原有 Video / Audio Stream
    for (unsigned int i = 0; i < inputCtx->nb_streams; ++i) {
        AVStream* inStream = inputCtx->streams[i];
        AVStream* outStream = avformat_new_stream(outputCtx, nullptr);

        if (!outStream) {
            std::cerr << "Could not create output stream." << std::endl;
            avformat_close_input(&inputCtx);
            avformat_free_context(outputCtx);
            return -1;
        }

        ret = avcodec_parameters_copy(outStream->codecpar, inStream->codecpar);

        if (ret < 0) {
            PrintError("avcodec_parameters_copy failed", ret);
            avformat_close_input(&inputCtx);
            avformat_free_context(outputCtx);
            return -1;
        }

        outStream->time_base = inStream->time_base;
        outStream->codecpar->codec_tag = 0;
    }

    // 创建 DATA Stream
    AVStream* dataStream = avformat_new_stream(outputCtx, nullptr);

    if (!dataStream) {
        std::cerr << "Could not create DATA stream." << std::endl;
        avformat_close_input(&inputCtx);
        avformat_free_context(outputCtx);
        return -1;
    }

    dataStream->codecpar->codec_type = AVMEDIA_TYPE_DATA;
    dataStream->codecpar->codec_id = AV_CODEC_ID_BIN_DATA;
    dataStream->codecpar->codec_tag = 0;

    // DATA Stream 以视频帧为时间单位
    dataStream->time_base = av_inv_q(frameRate);

    std::cout << "\nDATA Stream created:\n";
    std::cout << "  index     = " << dataStream->index << "\n";
    std::cout << "  type      = DATA\n";
    std::cout << "  codec_id  = " << dataStream->codecpar->codec_id << "\n";
    std::cout << "  codec_tag = 0x" << std::hex << dataStream->codecpar->codec_tag << std::dec << "\n";
    std::cout << "  timebase  = " << dataStream->time_base.num << "/" << dataStream->time_base.den << "\n";

    // 打开输出文件
    if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputCtx->pb, outputFile, AVIO_FLAG_WRITE);

        if (ret < 0) {
            PrintError("avio_open failed", ret);
            avformat_close_input(&inputCtx);
            avformat_free_context(outputCtx);
            return -1;
        }
    }

    // 写 MP4 Header
    ret = avformat_write_header(outputCtx, nullptr);

    if (ret < 0) {
        PrintError("avformat_write_header failed", ret);
        avformat_close_input(&inputCtx);

        if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outputCtx->pb);

        avformat_free_context(outputCtx);
        return -1;
    }

    // 保存所有 JSON 数据
    json allJson = json::array();

    AVPacket* packet = av_packet_alloc();

    if (!packet) {
        std::cerr << "Could not allocate packet." << std::endl;
        return -1;
    }

    int64_t frameIndex = 0;
    int64_t dataPacketIndex = 0;
    int64_t dataPts = 0;

    // 每 0.5 秒修改一次数据
    const double changeInterval = 0.5;

    // 读取输入 Packet
    while (av_read_frame(inputCtx, packet) >= 0) {

        // ====================================================
        // Video Packet
        // ====================================================
        if (packet->stream_index == videoStreamIndex) {

            int64_t videoPts = packet->pts;

            double timeSeconds = 0.0;

            if (videoPts != AV_NOPTS_VALUE)
                timeSeconds = videoPts * av_q2d(videoStream->time_base);
            else
                timeSeconds = static_cast<double>(frameIndex) / fps;

            // 每 0.5 秒修改一次 data_id
            int64_t dataId = static_cast<int64_t>(timeSeconds / changeInterval);

            // =================================================
            // 生成 JSON
            // =================================================
            json data;

            data["frame"] = frameIndex;
            data["pts"] = videoPts;
            data["time_ms"] = static_cast<int64_t>(timeSeconds * 1000.0);
            data["data_id"] = dataId;

            data["value"] = 1000 + dataId;
            data["speed"] = 60.0 + dataId;
            data["temperature"] = 20.0 + dataId * 0.5;

            data["message"] = "test_data_" + std::to_string(dataId);

            data["longitude"] = 116.123456 + dataId * 0.001;
            data["latitude"] = 39.123456 + dataId * 0.001;

            allJson.push_back(data);

            std::string jsonString = data.dump();

            // =================================================
            // 创建 DATA Packet
            // =================================================
            AVPacket* dataPacket = av_packet_alloc();

            if (!dataPacket) {
                std::cerr << "Could not allocate DATA packet." << std::endl;
                av_packet_unref(packet);
                break;
            }

            ret = av_new_packet(dataPacket, static_cast<int>(jsonString.size()));

            if (ret < 0) {
                PrintError("av_new_packet failed", ret);
                av_packet_free(&dataPacket);
                av_packet_unref(packet);
                break;
            }

            std::memcpy(dataPacket->data, jsonString.data(), jsonString.size());

            dataPacket->stream_index = dataStream->index;

            // DATA PTS 使用连续帧号
            dataPacket->pts = dataPts;
            dataPacket->dts = dataPts;
            dataPacket->duration = 1;
            dataPacket->pos = -1;

            ret = av_interleaved_write_frame(outputCtx, dataPacket);

            if (ret < 0) {
                PrintError("Write DATA packet failed", ret);
                av_packet_free(&dataPacket);
                av_packet_unref(packet);
                break;
            }

            // 打印前 20 条 DATA
            if (dataPacketIndex < 20) {
                std::cout << "\nDATA #" << dataPacketIndex << "\n";
                std::cout << "  frame       = " << frameIndex << "\n";
                std::cout << "  video pts   = " << videoPts << "\n";
                std::cout << "  data pts    = " << dataPts << "\n";
                std::cout << "  time        = " << timeSeconds << " s\n";
                std::cout << "  data_id     = " << dataId << "\n";
                std::cout << "  json        = " << jsonString << "\n";
            }

            av_packet_free(&dataPacket);

            ++dataPts;
            ++dataPacketIndex;
            ++frameIndex;
        }

        // ====================================================
        // 写原始 Video / Audio Packet
        // ====================================================
        int inputStreamIndex = packet->stream_index;

        AVStream* inStream = inputCtx->streams[inputStreamIndex];
        AVStream* outStream = outputCtx->streams[inputStreamIndex];

        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

        packet->pos = -1;

        ret = av_interleaved_write_frame(outputCtx, packet);

        if (ret < 0) {
            PrintError("Write original packet failed", ret);
            av_packet_unref(packet);
            break;
        }

        av_packet_unref(packet);
    }

    // 写 Trailer
    ret = av_write_trailer(outputCtx);

    if (ret < 0)
        PrintError("av_write_trailer failed", ret);

    // ========================================================
    // 保存 JSON 文件
    // ========================================================
    std::ofstream jsonOutput(jsonFile);

    if (!jsonOutput.is_open()) {
        std::cerr << "Could not create JSON file: " << jsonFile << std::endl;
    }
    else {
        jsonOutput << allJson.dump(4);
        jsonOutput.close();

        std::cout << "\nJSON file generated: " << jsonFile << std::endl;
    }

    // ========================================================
    // 释放资源
    // ========================================================
    av_packet_free(&packet);
    avformat_close_input(&inputCtx);

    if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&outputCtx->pb);

    avformat_free_context(outputCtx);

    // ========================================================
    // 输出结果
    // ========================================================
    std::cout << "\n========================================\n";
    std::cout << "Finished\n";
    std::cout << "========================================\n";
    std::cout << "Video frames : " << frameIndex << "\n";
    std::cout << "Data packets : " << dataPacketIndex << "\n";
    std::cout << "Output MP4   : " << outputFile << "\n";
    std::cout << "Output JSON  : " << jsonFile << "\n";

    return 0;
}