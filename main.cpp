#include "CustomMetadata/json_stream_data.h"
#include "event_loop.h"
#include "player.h"

int main(int argc, char** argv) {

	const char* mediaPath = "./walking-dead-pb-stream.mp4";
	Player player;
    if (player.Open(mediaPath) < 0) {
        av_log(nullptr, AV_LOG_ERROR, "Failed to open media file: %s, exiting\n", mediaPath);
        player.Close();
        return -1;
    }
	player.Start();

	EventLoop loop;
	return loop.Run(player);
}