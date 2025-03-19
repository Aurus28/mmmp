
#include <gst/gst.h>       
#include <systemd/sd-bus.h>
#include <iostream>
#include <cstring>
#include <atomic>
#include <thread>

// global variables / values
const bool can_quit = false;
const bool can_raise = false;
const char *sd_bus_prefix = "[D-Bus] ";
const char *thread_prefix = "[Thread] ";
std::atomic<bool> exit_flag(false); // atomic -> safer when threading


// method declarations
void inputListener();
const char* message_type_to_str(int type);
void dump_message_process(sd_bus_message *msg);


// vtables
sd_bus_vtable vtable[] {
	SD_BUS_VTABLE_START(0),

	SD_BUS_VTABLE_END
};

int main() {
	// local variables / values
	int error_code;	
	sd_bus *bus = nullptr;
	sd_bus_slot *slot = nullptr;
	sd_bus_message *processed_message = nullptr;
	const char *name = "org.mpris.MediaPlayer2.Mmmp";


	// create thread(s)
	std::thread inputThread(inputListener);
	std::cout << thread_prefix << "Input listener thread started" << std::endl;

	
	// register bus
	error_code = sd_bus_default_user(&bus);
	if (error_code < 0) {
		std::cout << sd_bus_prefix << "Failed to register bus: " << strerror(-error_code) << std::endl;
	} else {
		std::cout << sd_bus_prefix << "Registered bus" << std::endl;
	}


	// request name
	error_code = sd_bus_request_name(bus, name, 0);
	if (error_code < 0) {
		std::cout << sd_bus_prefix << "Failed to request name \"" << name << "\": " << strerror(-error_code) << std::endl;
	} else {
		std::cout << sd_bus_prefix << "Requested name \"" << name <<"\"" << std::endl;
	}

	
	// add vtable
	error_code = sd_bus_add_object_vtable(bus, &slot, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2", vtable, NULL);
	if (error_code < 0) {
		std::cout << sd_bus_prefix << "Failed to add vtable: " << strerror(-error_code) << std::endl; 
	} else {
		std::cout << sd_bus_prefix << "Sucessfully added vtable" << std::endl;
	}


	// event loop to check for sd_bus_messages
	while(!exit_flag) {
		error_code = sd_bus_process(bus, &processed_message);
		if (error_code < 0) {
			std::cout << sd_bus_prefix << "Error processing message: " << strerror(-error_code) << std::endl;
		} else if(error_code == 0) {
			sd_bus_wait(bus, 50000);
		} else {
			dump_message_process(processed_message);
		}
	}


	// clean up to prevent resource leaks
	sd_bus_unref(bus);
	std::cout << sd_bus_prefix << "Bus unreferenced" << std::endl;
	inputThread.join();
	std::cout << thread_prefix << "Input listener thread joined" << std::endl;

	// return to exit
	return 0;
}


// methods
void inputListener() {
	std::string input;

	while(!exit_flag) {
		std::cin >> input;
		std::cout << thread_prefix << "Recived cin input: " << input << std::endl;
		if(input == "exit") {
			exit_flag = true;
			break;
		}
	}
}

const char* message_type_to_str(int type) {
	switch(type) {
	case SD_BUS_MESSAGE_METHOD_CALL: 
		return "SD_BUS_MESSAGE_METHOD_CALL";
	case SD_BUS_MESSAGE_METHOD_RETURN: 
		return "SD_BUS_MESSAGE_METHOD_RETURN";
	case SD_BUS_MESSAGE_METHOD_ERROR: 
		return "SD_BUS_MESSAGE_METHOD_ERROR";
	case SD_BUS_MESSAGE_SIGNAL: 
		return "SD_BUS_MESSAGE_SIGNAL";
	default: 
		return "UNKNOWN";
	}
}


void dump_message_process(sd_bus_message *msg) {
	int err;
	uint8_t processed_message_type;

	std::cout << sd_bus_prefix << "Processed a message:" << std::endl;

	// print message info
	if(msg != NULL) {
		err = sd_bus_message_get_type(msg, &processed_message_type);
		std::cout << "Message type read with return value: " << strerror(-err) << std::endl;

		if(err < 0 || processed_message_type == NULL) {
			std::cout << "something went wrong while reading message type" << std::endl;
			return;
		}

		int temp = (int)(processed_message_type);
		std::cout << sd_bus_prefix << "Message type def you will never get a uint8_t* ined: " << message_type_to_str(temp) << std::endl;

		sd_bus_message_dump(msg, NULL, 0);
	} else {
		std::cout << "Processed message was NULL" << std::endl;
	}
}
