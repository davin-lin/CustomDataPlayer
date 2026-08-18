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

#include "data.pb.h"

static void print_error(const char* msg, int ret)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, errbuf, sizeof(errbuf));
    std::cerr << msg << ": " << errbuf << std::endl;
}

static int CreateProtobufStreamMP4()
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    const char* inputFile = "D:\\tmp\\walking-dead.mp4";
    const char* outputFile = "D:\\tmp\\walking-dead-pb-stream.mp4";
    const char* pbFile = "D:\\tmp\\data.bin";

    AVFormatContext* inputCtx = nullptr;
    int ret = avformat_open_input(&inputCtx, inputFile, nullptr, nullptr);

    if (ret < 0) {
        print_error("avformat_open_input failed", ret);
        return -1;
    }

    ret = avformat_find_stream_info(inputCtx, nullptr);

    if (ret < 0) {
        print_error("avformat_find_stream_info failed", ret);
        avformat_close_input(&inputCtx);
        return -1;
    }

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

    AVRational frameRate = videoStream->avg_frame_rate;

    if (frameRate.num == 0 || frameRate.den == 0)
        frameRate = videoStream->r_frame_rate;

    double fps = av_q2d(frameRate);

    if (fps <= 0.0) {
        std::cerr << "Invalid FPS." << std::endl;
        avformat_close_input(&inputCtx);
        return -1;
    }

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

    AVFormatContext* outputCtx = nullptr;
    ret = avformat_alloc_output_context2(&outputCtx, nullptr, nullptr, outputFile);

    if (ret < 0 || !outputCtx) {
        print_error("avformat_alloc_output_context2 failed", ret);
        avformat_close_input(&inputCtx);
        return -1;
    }

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
            print_error("avcodec_parameters_copy failed", ret);
            avformat_close_input(&inputCtx);
            avformat_free_context(outputCtx);
            return -1;
        }

        outStream->time_base = inStream->time_base;
        outStream->codecpar->codec_tag = 0;
    }

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

    dataStream->time_base = av_inv_q(frameRate);

    std::cout << "\nDATA Stream created:\n";
    std::cout << "  index     = " << dataStream->index << "\n";
    std::cout << "  type      = DATA\n";
    std::cout << "  codec_id  = " << dataStream->codecpar->codec_id << "\n";
    std::cout << "  codec_tag = 0x" << std::hex << dataStream->codecpar->codec_tag << std::dec << "\n";
    std::cout << "  timebase  = " << dataStream->time_base.num << "/" << dataStream->time_base.den << "\n";

    if (!(outputCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputCtx->pb, outputFile, AVIO_FLAG_WRITE);

        if (ret < 0) {
            print_error("avio_open failed", ret);
            avformat_close_input(&inputCtx);
            avformat_free_context(outputCtx);
            return -1;
        }
    }

    ret = avformat_write_header(outputCtx, nullptr);

    if (ret < 0) {
        print_error("avformat_write_header failed", ret);
        avformat_close_input(&inputCtx);

        if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&outputCtx->pb);

        avformat_free_context(outputCtx);
        return -1;
    }

    customstream::FrameDataList allPb;

    AVPacket* packet = av_packet_alloc();

    if (!packet) {
        std::cerr << "Could not allocate packet." << std::endl;
        return -1;
    }

    int64_t frameIndex = 0;
    int64_t dataPacketIndex = 0;
    int64_t dataPts = 0;

    const double changeInterval = 0.5;

    while (av_read_frame(inputCtx, packet) >= 0) {

        if (packet->stream_index == videoStreamIndex) {

            int64_t videoPts = packet->pts;

            double timeSeconds = 0.0;

            if (videoPts != AV_NOPTS_VALUE)
                timeSeconds = videoPts * av_q2d(videoStream->time_base);
            else
                timeSeconds = static_cast<double>(frameIndex) / fps;

            int64_t dataId = static_cast<int64_t>(timeSeconds / changeInterval);

            customstream::FrameData* data = allPb.add_items();

            data->set_name("protobuf");
            data->set_frame(frameIndex);
            data->set_pts(videoPts);
            data->set_time_ms(static_cast<int64_t>(timeSeconds * 1000.0));
            data->set_data_id(dataId);

            data->set_value(1000 + dataId);
            data->set_speed(60.0 + dataId);
            data->set_temperature(20.0 + dataId * 0.5);

            data->set_message("test_data_" + std::to_string(dataId));

            data->set_longitude(116.123456 + dataId * 0.001);
            data->set_latitude(39.123456 + dataId * 0.001);

            std::string pbString;
            data->SerializeToString(&pbString);

            AVPacket* dataPacket = av_packet_alloc();

            if (!dataPacket) {
                std::cerr << "Could not allocate DATA packet." << std::endl;
                av_packet_unref(packet);
                break;
            }

            ret = av_new_packet(dataPacket, static_cast<int>(pbString.size()));

            if (ret < 0) {
                print_error("av_new_packet failed", ret);
                av_packet_free(&dataPacket);
                av_packet_unref(packet);
                break;
            }

            std::memcpy(dataPacket->data, pbString.data(), pbString.size());

            dataPacket->stream_index = dataStream->index;

            dataPacket->pts = dataPts;
            dataPacket->dts = dataPts;
            dataPacket->duration = 1;
            dataPacket->pos = -1;

            ret = av_interleaved_write_frame(outputCtx, dataPacket);

            if (ret < 0) {
                print_error("Write DATA packet failed", ret);
                av_packet_free(&dataPacket);
                av_packet_unref(packet);
                break;
            }

            if (dataPacketIndex < 20) {
                std::cout << "\nDATA #" << dataPacketIndex << "\n";
                std::cout << "  frame       = " << frameIndex << "\n";
                std::cout << "  video pts   = " << videoPts << "\n";
                std::cout << "  data pts    = " << dataPts << "\n";
                std::cout << "  time        = " << timeSeconds << " s\n";
                std::cout << "  data_id     = " << dataId << "\n";
                std::cout << "  pb size     = " << pbString.size() << " bytes\n";
                std::cout << "  DebugString = " << data->DebugString();
            }

            av_packet_free(&dataPacket);

            ++dataPts;
            ++dataPacketIndex;
            ++frameIndex;
        }

        int inputStreamIndex = packet->stream_index;

        AVStream* inStream = inputCtx->streams[inputStreamIndex];
        AVStream* outStream = outputCtx->streams[inputStreamIndex];

        av_packet_rescale_ts(packet, inStream->time_base, outStream->time_base);

        packet->pos = -1;

        ret = av_interleaved_write_frame(outputCtx, packet);

        if (ret < 0) {
            print_error("Write original packet failed", ret);
            av_packet_unref(packet);
            break;
        }

        av_packet_unref(packet);
    }

    ret = av_write_trailer(outputCtx);

    if (ret < 0)
        print_error("av_write_trailer failed", ret);

    std::ofstream pbOutput(pbFile, std::ios::binary);

    if (!pbOutput.is_open()) {
        std::cerr << "Could not create PB file: " << pbFile << std::endl;
    }
    else {
        allPb.SerializeToOstream(&pbOutput);
        pbOutput.close();

        std::cout << "\nPB file generated: " << pbFile << std::endl;
        std::cout << "Total FrameData items: " << allPb.items_size() << std::endl;
    }

    av_packet_free(&packet);
    avformat_close_input(&inputCtx);

    if (!(outputCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&outputCtx->pb);

    avformat_free_context(outputCtx);

    std::cout << "\n========================================\n";
    std::cout << "Finished\n";
    std::cout << "========================================\n";
    std::cout << "Video frames : " << frameIndex << "\n";
    std::cout << "Data packets : " << dataPacketIndex << "\n";
    std::cout << "Output MP4   : " << outputFile << "\n";
    std::cout << "Output PB    : " << pbFile << "\n";

    google::protobuf::ShutdownProtobufLibrary();

    return 0;
}
