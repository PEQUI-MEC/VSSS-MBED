#include <string>
#include "Messenger.h"
#include "PIN_MAP.h"
#include "SensorFusion.h"
#include "helper_functions.h"

#define PI 3.1415926f

void Messenger::send_msg(const std::string &msg, uint16_t addr) {
	XBeeLib::RemoteXBee802 remoteDevice = XBeeLib::RemoteXBee802(addr);
	xbee.send_data(remoteDevice, (const uint8_t *) msg.c_str(), (uint16_t ) msg.size(), true);
}

void Messenger::send_battery() {
	AnalogIn vin_all_cells(ALL_CELLS);
	float vbat = vin_all_cells.read() * (3.3f * 1470.0f/470.0f);
	float vbat_round = std::round(vbat*1000)/1000;
	std::string msg_bat = "B" + std::to_string(vbat_round);
	send_msg(msg_bat);
}


void Messenger::send_info(SensorFusion& sensors, RobotController& robot) {
	if (battery_requested) {
		send_battery();
		battery_requested = false;
	}
	std::string msg = str(robot.theta_error);
	send_msg(msg);
}

bool Messenger::is_delaying() {
	return delay_time > 0 && delay_timer.read() < delay_time;
}

void Messenger::update_by_messages(SensorFusion& sensors, RobotController& robot) {
	// send_msg("ok!");
	if (!message_buffer.empty()) {
		// send_msg("got msg");
		Message msg;
		message_buffer.pop(msg);
		send_msg(msg.to_str());

		switch (msg.type) {
			case 'E':
				if (msg.data_size == 3) {
					sensors.set_vision_data(msg.data[0], msg.data[1], msg.data[2]);
					break;
				} else {
					break;
				}
			case 'U':
				if (msg.data_size == 6) {
					robot.start_uvf_control(msg.data[0]/100, msg.data[1]/100,
						msg.data[2]/100, msg.data[3]/100, msg.data[4], msg.data[5]);
					break;
				} else {
					break;
				}
			case 'O':
				if (msg.data_size == 2) {
					robot.start_orientation_control(msg.data[0], msg.data[1]);
					break;
				} else {
					break;
				}
			case 'P':
				if (msg.data_size == 3) {
					robot.start_position_control(msg.data[0]/100, msg.data[1]/100, msg.data[2]);
					break;
				} else {
					break;
				}
			case 'V':
				if (msg.data_size == 2) {
					robot.start_vector_control(msg.data[0], msg.data[1]);
					break;
				} else {
					break;
				}
			case 'W':
				if (msg.data_size == 2) {
					robot.start_velocity_control(msg.data[1], msg.data[0]);
					break;
				} else {
					break;
				}
			case 'D':
				if (msg.data_size == 1) {
					delay_time = msg.data[0];
					delay_timer.reset();
					break;
				} else {
					break;
				}
			case 'B':
				battery_requested = true;
				break;
			default:
				break;
		}
	}
}

void Messenger::process_xbee_msgs() {
	xbee.process_rx_frames();
}

Message Messenger::parse(const std::string &msg) {
	Message parsed_message;

	unsigned int first_char_pos = 1;
	if (isalpha(msg[0])) {
		parsed_message.type = msg[0];
	} else {
		parsed_message.type = 'W';
		first_char_pos = 0;
	}

	unsigned int current_position = first_char_pos;
	int i = 0;
	while (current_position < msg.size()) {
		// send_msg(str(i) + ":");
		size_t delimiter_posision = msg.find(';', current_position);
		// send_msg(str(delimiter_posision));
		size_t last_position = (delimiter_posision == std::string::npos) ? msg.size() : delimiter_posision;
		// send_msg(str(last_position));
		parsed_message.data[i] = std::stof(msg.substr(current_position, last_position - current_position));
		// send_msg(msg.substr(current_position, last_position - current_position));
		// send_msg(str(parsed_message.data[i]));
		i += 1;
		current_position = last_position + 1;
		// send_msg(str(current_position));
	}

	parsed_message.data_size = i;

	return parsed_message;
}

void Messenger::parse_and_add_to_buffer(const uint8_t *const data, uint16_t len) {
	std::string msg = std::string((const char *) data, len);
	Message parsed_message = parse(msg);
	message_buffer.push(parsed_message);
}

Messenger * messenger = nullptr;

static void receive_cb(const XBeeLib::RemoteXBee802 &remote, bool broadcast,
					   const uint8_t *const data, uint16_t len) {
	if (len != 0) {
		//std::string msg = std::string((const char *) data, len);
		messenger->parse_and_add_to_buffer(data, len);
	}
}

Messenger::Messenger(char id, uint16_t xbee_addr)
		:  ID(id), xbee(RADIO_TX, RADIO_RX, RADIO_RESET, NC, NC, 115200) {

	messenger = this;

	delay_timer.start();

	setlocale(LC_ALL, "C");

	xbee.register_receive_cb(&receive_cb);
	XBeeLib::RadioStatus const radioStatus = xbee.init();
	MBED_ASSERT(radioStatus == XBeeLib::Success);
	xbee.set_network_address(xbee_addr);
}
