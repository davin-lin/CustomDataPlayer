#include "CustomMetadata/custom_data.h"
#include "CustomMetadata/json_data.h"
#include "CustomMetadata/proto_data.h"
#include "event_loop.h"
#include "player.h"

int main(int argc, char** argv) {

	//// Copy and mux with JSON metadata
	//if (copyMuxWithMetadata() < 0) {
	//	std::cerr << "Failed to copy and mux with JSON metadata." << std::endl;
	//	return -1;
	//}
	//// Verify JSON metadata
	//if (verifyMetadata() < 0) {
	//	std::cerr << "Failed to verify JSON metadata." << std::endl;
	//	return -1;
	//}
	//// Copy and mux with Protobuf metadata
	//if (copyMuxWithProtobufMeta() < 0) {
	//	std::cerr << "Failed to copy and mux with Protobuf metadata." << std::endl;
	//	return -1;
	//}
	//// Verify Protobuf metadata
	//if (verifyProtobufMeta() < 0) {
	//	std::cerr << "Failed to verify Protobuf metadata." << std::endl;
	//	return -1;
	//}

	const char* mediaPath = (argc > 1) ? argv[1] : DEFAULT_MEDIA_PATH;
	Player player;
	player.Open(mediaPath);
	player.Start();

	EventLoop loop;
	return loop.Run(player);
}