#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

//static AVDictionary* format_opts = nullptr;
static int g_framedrop = -1;


#define VIDEO_FRAME_QUEUE_SIZE 3
#define SUBTITLE_FRAME_QUEUE_SIZE 16
#define AUDIO_FRAME_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(AUDIO_FRAME_QUEUE_SIZE, FFMAX(VIDEO_FRAME_QUEUE_SIZE, SUBTITLE_FRAME_QUEUE_SIZE))

/* no AV sync correction is done if below the minimum AV sync threshold */

#define AV_SYNC_THRESHOLD_MIN 0.04

/* AV sync correction is done if above the maximum AV sync threshold */

#define AV_SYNC_THRESHOLD_MAX 0.1

/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */

#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1

/* no AV correction is done if too big error */

#define AV_NOSYNC_THRESHOLD 10.0

/* Minimum SDL audio buffer size, in samples. */

#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */

#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30