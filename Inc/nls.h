/*
 * nls.h
 *
 *  2024 NOV 16, v1.00
 *  	Ported from JBC controller source code, tailored to the new hardware
 *  2025 MAR 06, v1.01
 *  	Added 'erase FLASH?', 'FLASH erase error', 'FLASH format complete' and 'INFO' messages
 *  	Changed message 'FLASH debug' to 'FLASH erase'
 *  	Changed all 'EEPROM' to 'FLASH'
 *  	Removed the messages that no longer used: 'No directory', 'Delete file?', 'Failed mount SD', 'NO config file',
 *  	'No lang. specified', 'No memory', 'Inconsistent lang'
 *  2025 NOV 02, v1.03
 *  	Added "Fan voltage" menu item into "HOT GUN setup" menu
 *  2025 NOV 11, v1.03
 *  	Added "ambient"  menu item into "Parameters" menu
 *  2026 FEB 18, v1.04
 *  	Added "Encoder mode" menu into Hot Air Gun setup
 *  	Added "fan speed" and "temperature" messages
 */

#ifndef MSG_NLS_H_
#define MSG_NLS_H_

#include <string>

typedef enum e_msg { MSG_MENU_MAIN, MSG_MENU_SETUP = 10, MSG_MENU_T12 = 10+15, MSG_MENU_JBC = 10+15+11, MSG_MENU_GUN = 10+15+11+6,
					 MSG_MENU_CALIB = 10+15+11+6+9, MSG_PID_MENU = 10+15+11+6+9+5, MSG_FLASH_MENU = 10+15+11+6+9+5+5,
					MSG_ON = 10+15+11+6+9+5+5+5, MSG_OFF, MSG_FAN, MSG_PWR,
					MSG_REF_POINT, MSG_REED, MSG_TILT, MSG_DEG, MSG_MINUTES, MSG_SECONDS,
					MSG_CW, MSG_CCW, MSG_SET, MSG_INFO, MSG_ERROR, MSG_TUNE_PID, MSG_SELECT_TIP,
					MSG_FLASH_READ_ERR, MSG_FLASH_WRITE_ERR, MSG_FLASH_ERASE_ERR, MSG_FORMAT_COMPLETE, MSG_NO_TIP_LIST, MSG_DO_FORMAT_FLASH,
					MSG_DO_ERASE_FLASH, MSG_FORMAT_FAILED, MSG_SAVE_ERROR, MSG_HOT_AIR_GUN, MSG_T12_IRON, MSG_JBC_IRON, MSG_SAVE_Q, MSG_YES, MSG_NO,
					MSG_FLASH_ERASE, MSG_DSPL_IPS, MSG_DSPL_TFT, MSG_GUN_STBY,
					MSG_UPDATE_FLASH, MSG_ENC_FAN, MSG_ENC_TEMP,
					MSG_LAST,
					MSG_ACTIVATE_TIPS 	= MSG_MENU_MAIN + 3,
					MSG_ABOUT 			= MSG_MENU_MAIN + 8,
					MSG_AUTO			= MSG_MENU_CALIB + 1,
					MSG_MANUAL			= MSG_MENU_CALIB + 2
} t_msg_id;

typedef struct s_msg_nls {
	const char		*msg;
	std::string		msg_nls;
} t_msg;

class NLS_MSG {
	public:
		NLS_MSG()											{ }
		void			activate(bool use_nls)				{ this->use_nls = use_nls; }
		const char*		msg(t_msg_id id);
		std::string		str(t_msg_id id);
		uint8_t			menuSize(t_msg_id id);
		bool			set(std::string& parameter, std::string& value, std::string& parent);
	protected:
		bool	use_nls		= false;
		t_msg		message[MSG_LAST] = {
				// MAIN MENU
				{"Main Menu",		std::string()},			// Title is the first element of each menu
				{"parameters",		std::string()},
				{"change T12 tip",	std::string()},
				{"activate tips",	std::string()},			// Change MSG_ACTIVATE_TIPS if new item menu inserted
				{"T12 setup",		std::string()},
				{"JBC setup",		std::string()},
				{"HOT GUN setup",	std::string()},
				{"reset config",	std::string()},
				{"about",			std::string()},			// Change MSG_ABOUT if new item menu inserted
				{"quit",			std::string()},
				// SETUP MENU
				{"Parameters",		std::string()},			// Title
				{"units",			std::string()},
				{"buzzer",			std::string()},
				{"upper encoder",	std::string()},
				{"lower encoder",	std::string()},
				{"temp. step",		std::string()},
				{"brightness",		std::string()},			// Change in-place menu item
				{"ambient",			std::string()},			// Change in-place menu item
				{"rotation",		std::string()},			// Change in-place menu item
				{"language",		std::string()},			// Change in-place menu item
				{"display type",	std::string()},
				{"max temperature",	std::string()},
				{"tune PID",		std::string()},
				{"save",			std::string()},
				{"cancel",			std::string()},
				// T12 IRON MENU
				{"T12 iron setup",	std::string()},			// Title
				{"switch type",		std::string()},
				{"auto start",		std::string()},
				{"auto off",		std::string()},
				{"standby temp.",	std::string()},
				{"standby time",	std::string()},
				{"boost temp.",		std::string()},
				{"boost time",		std::string()},
				{"save",			std::string()},
				{"calibrate tip",	std::string()},
				{"back to menu",	std::string()},
				// JBC IRON MENU
				{"JBC iron setup",	std::string()},			// Title
				{"auto off",		std::string()},
				{"standby temp.",	std::string()},
				{"save",			std::string()},
				{"calibrate tip",	std::string()},
				{"back to menu",	std::string()},
				// HOT AIR GUN MENU
				{"HOT GUN setup",	std::string()},			// Title
				{"fast chill",		std::string()},
				{"standby time",	std::string()},
				{"standby temp.",	std::string()},
				{"fan voltage",		std::string()},
				{"encoder mode",	std::string()},
				{"save",			std::string()},
				{"calibrate gun",	std::string()},
				{"back to menu",	std::string()},
				// IRON TIP CALIBRATION MENU
				{"Calibrate",		std::string()},			// Title
				{"automatic",		std::string()},			// Change MSG_AUTO if new item menu inserted
				{"manual",			std::string()},			// Change MSG_MANUAL if new item menu inserted
				{"clear",			std::string()},
				{"quit",			std::string()},
				// PID Tune Menu
				{"Tune PID",		std::string()},			// Title
				{"T12 PID",			std::string()},
				{"JBC PID",			std::string()},
				{"Gun PID",			std::string()},
				{"back to menu",	std::string()},
				// Configuration manage menu
				{"Manage config",	std::string()},			// Title
				{"Load lang data",	std::string()},
				{"Load config",		std::string()},
				{"Save config",		std::string()},
				{"quit",			std::string()},
				// SINGLE MESSAGE STRINGS
				{"ON",				std::string()},
				{"OFF",				std::string()},
				{"Fan:",			std::string()},
				{"pwr:",			std::string()},
				{"Ref. #",			std::string()},
				{"REED",			std::string()},
				{"TILT",			std::string()},
				{"deg.",			std::string()},
				{"min",				std::string()},
				{"sec",				std::string()},
				{"cw",				std::string()},
				{"ccw",				std::string()},
				{"Set:",			std::string()},
				{"INFO",			std::string()},
				{"ERROR",			std::string()},
				{"Tune PID",		std::string()},
				{"Select tip",		std::string()},
				{"FLASH read error",		std::string()},
				{"FLASH write error",		std::string()},
				{"FLASH erase error",		std::string()},
				{"FLASH format complete",	std::string()},
				{"No Tip list",				std::string()},
				{"format FLASH?",			std::string()},
				{"erase FLASH?",			std::string()},
				{"Failed to format FLASH",	std::string()},
				{"saving configuration",	std::string()},
				{"Hot Gun",					std::string()},
				{"T12 iron",				std::string()},
				{"JBC iron",				std::string()},
				{"Save?",					std::string()},
				{"Yes",						std::string()},
				{"No",						std::string()},
				{"FLASH erase",				std::string()},
				{"IPS",						std::string()},
				{"TFT",						std::string()},
				{"standby",					std::string()},
				{"updating flash",			std::string()},
				{"fan speed",				std::string()},
				{"temperature",				std::string()}
		};
		const t_msg_id menu[7] = { MSG_MENU_MAIN, MSG_MENU_SETUP, MSG_MENU_T12, MSG_MENU_JBC, MSG_MENU_GUN, MSG_MENU_CALIB, MSG_PID_MENU };
};

#endif
