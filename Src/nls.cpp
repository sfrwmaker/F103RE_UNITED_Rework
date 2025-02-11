/*
 * nls.cpp
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 */

#include "nls.h"
#include "vars.h"

const char* NLS_MSG::msg(t_msg_id id) {
	if (id < MSG_LAST) {
		if (use_nls && message[(uint8_t)id].msg_nls.length() > 0)
			return message[(uint8_t)id].msg_nls.c_str();
		else
			return message[(uint8_t)id].msg;
	}
	return 0;
}

std::string NLS_MSG::str(t_msg_id id) {
	std::string ret = std::string();
	if (id < MSG_LAST) {
		if (use_nls && message[(uint8_t)id].msg_nls.length() > 0)
			ret = message[(uint8_t)id].msg_nls;
		else
			ret = std::string(message[(uint8_t)id].msg);
	}
	return ret;
}

// Each menu starts with menu title, so actual menu size is less by 1
uint8_t NLS_MSG::menuSize(t_msg_id id) {
	uint8_t ret = 0;
	switch (id) {
		case MSG_MENU_MAIN:
			ret = MSG_MENU_SETUP - MSG_MENU_MAIN -1;
			break;
		case MSG_MENU_SETUP:
			ret = MSG_MENU_T12 - MSG_MENU_SETUP -1;
			break;
		case MSG_MENU_T12:
			ret = MSG_MENU_JBC - MSG_MENU_T12 -1;
			break;
		case MSG_MENU_JBC:
			ret = MSG_MENU_GUN - MSG_MENU_JBC -1;
			break;
		case MSG_MENU_GUN:
			ret = MSG_MENU_CALIB - MSG_MENU_GUN - 1;
			break;
		case MSG_MENU_CALIB:
			ret = MSG_PID_MENU - MSG_MENU_CALIB -1;
			break;
		case MSG_PID_MENU:
			ret = MSG_FLASH_MENU - MSG_PID_MENU -1;
			break;
		case MSG_FLASH_MENU:
			ret = MSG_ON - MSG_FLASH_MENU -1;
		default:
			break;
	}
	return ret;
}

bool NLS_MSG::set(std::string& parameter, std::string& value, std::string& parent) {
	uint8_t first = 0;
	uint8_t last = MSG_LAST;
	if (!parent.empty()) {
		if (parent.compare(standalone_msg) == 0) { 			// standalone_msg defined in vars.h
			first	= (uint8_t)MSG_ON;
		} else {											// Perhaps, menu name specified
			for (uint8_t m = 0; m < sizeof(menu)/sizeof(t_msg_id); ++m) {
				const char *m_name = message[(uint8_t)menu[m]].msg;
				if (parent.compare(m_name) == 0) {			// Menu has been found, limit search context
					first	= (uint8_t)menu[m];
					last	= first + menuSize(menu[m]) + 1; // The first menu item is menu title
					break;
				}
			}
		}
	}
	for (uint8_t i = first; i < last; ++i) {
		if (parameter.compare(message[i].msg) == 0) {		// Parameter has been found
			message[i].msg_nls = value;
			use_nls = true;									// At least one message was loaded
			return true;
		}
	}
	return false;											// Parameter not found
}
