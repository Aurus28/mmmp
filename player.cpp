
#include <gst/gst.h>       
#include <systemd/sd-bus.h>
#include <iostream>
#include <cstring>

// Global GStreamer pipeline (audio player)
GstElement *pipeline = nullptr;

static int handle_get_all_properties(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
	sd_bus_message *reply = NULL;
    	int r;

	//debugging
	std::cout << "handle_get_all_properties invoked" << std::endl;

    	// Create a new reply message
    	r = sd_bus_message_new_method_return(m, &reply);
    	if (r < 0) return r;

    	// Start a dictionary
    	r = sd_bus_message_open_container(reply, 'a', "{sv}");
    	if (r < 0) return r;

    	// Example property: PlaybackStatus (string)
    	r = sd_bus_message_append(reply, "{sv}", "PlaybackStatus", "s", "Playing");
    	if (r < 0) return r;

    	// Example property: LoopStatus (string)
    	r = sd_bus_message_append(reply, "{sv}", "LoopStatus", "s", "None");
    	if (r < 0) return r;

	// set interface name
    	r = sd_bus_message_append(reply, "{sv}", "Interface", "s", "Mmmp");
    	if (r < 0) return r;

    	// Close the dictionary
    	r = sd_bus_message_close_container(reply);
    	if (r < 0) return r;

    	// Send the reply
    	return sd_bus_send(NULL, reply, NULL);
}
// Handles "Play" MPRIS command
static int handle_play(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {

	std::cout << "[MPRIS] Play command received\n";
	sd_bus_message_dump(m, stdout, 0);
	std::cout << sd_bus_message_get_member(m) << std::endl;

    	if (pipeline) {
		gst_element_set_state(pipeline, GST_STATE_PLAYING); // Start playback
    	} else {
		return -1;
    	}
    	return sd_bus_reply_method_return(m, ""); // Send an empty reply to confirm execution
}

// Handles "Pause" MPRIS command
static int handle_pause(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
	const char *str;
	int r;

	r=sd_bus_message_read_basic(m, 's', &str);
	std::cout << "[MPRIS] Pause command received\n";
	std::cout <<  str << std::endl;
    	if (pipeline) {
		gst_element_set_state(pipeline, GST_STATE_PAUSED); // Pause playback
    	}
    	return sd_bus_reply_method_return(m, "");
}

// Define MPRIS D-Bus interface
static const sd_bus_vtable mpris_vtable[] = {
	SD_BUS_VTABLE_START(0),
    	SD_BUS_METHOD("Play", "", "", handle_play, SD_BUS_VTABLE_UNPRIVILEGED),
    	SD_BUS_METHOD("Pause", "", "", handle_pause, SD_BUS_VTABLE_UNPRIVILEGED),
    	SD_BUS_VTABLE_END
};

static const sd_bus_vtable properties_vtable[] = {
	SD_BUS_VTABLE_START(1),
	SD_BUS_METHOD("GetAll", "s", "a{sv}", handle_get_all_properties, SD_BUS_VTABLE_UNPRIVILEGED),
    	SD_BUS_VTABLE_END
};

int main(int argc, char *argv[]) {
	// Check if a file path is provided
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <path to audio file>\n";
	    	return 1;
	}
	
	const char *file_path = argv[1];
	
	// Initialize GStreamer
	gst_init(&argc, &argv);
	
	// Create a playbin element (handles file playback)
	//pipeline = gst_element_factory_make("playbin", "player");
	std::string uri = "playbin uri=file://" + std::string(file_path);
	pipeline = gst_parse_launch(uri.c_str(), NULL);
	if (!pipeline) {
		std::cerr << "Failed to create GStreamer pipeline.\n";
	    	return 1;
	}
	
	// Set the file URI for playback
	//std::string uri = "file://" + std::string(file_path);
	//g_object_set(pipeline, "uri", uri.c_str(), NULL);
	
	// Start playing the audio file
	gst_element_set_state(pipeline, GST_STATE_PLAYING);
	std::cout << "[GStreamer] Playing: " << file_path << "\n";
	
	// Initialize D-Bus communication for MPRIS
	sd_bus *bus = nullptr;
	sd_bus_slot *slot = nullptr;
	int r = sd_bus_open_user(&bus); // Connect to user session bus
	if (r < 0) {
		std::cerr << "Failed to connect to D-Bus: " << strerror(-r) << "\n";
	    	return 1;
	}

	r = sd_bus_request_name(bus, "org.mpris.MediaPlayer2.Mmmp", SD_BUS_NAME_REPLACE_EXISTING);
	if (r < 0) {
		std::cerr << "Failed to request name: " << strerror(-r) << "\n";
	    	return 1;
	}

	// Register MPRIS interface on the session bus
	r = sd_bus_add_object_vtable(bus, &slot, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", mpris_vtable, NULL);
	if (r < 0) {
		std::cerr << "Failed to register MPRIS interface: " << strerror(-r) << "\n";
	    	return 1;
	}
	
	//register d-Bus propteties interface
	r = sd_bus_add_object_vtable(bus, &slot, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer",properties_vtable, NULL);
	if (r < 0) {
		std::cerr << "Failed to register Properties interface: " << strerror(-r) << "\n";
	    	return 1;
	}
	
	std::cout << "[MPRIS] Registered on D-Bus.\n";
	std::cout << "Use `playerctl play-pause` or ther MPRIS clients to control playback.\n";
	
	// Event loop to keep the application running and respond to D-Bus calls
	while (true) {
		r = sd_bus_process(bus, NULL);

		r = sd_bus_wait(bus, (uint64_t)-1);
	        if (r < 0) {
			std::cerr << "D-Bus waiting error: " << strerror(-r) << "\n";
			break;
		}
	}

	// Cleanup
	gst_element_set_state(pipeline, GST_STATE_NULL);
	gst_object_unref(pipeline);
	sd_bus_unref(bus);
	
	return 0;
}


