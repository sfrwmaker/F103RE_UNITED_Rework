/*
 * cfgtypes.h
 *
 *  2024 NOV 16
 *  	Ported from JBC controller source code, tailored to the new hardware
 */

#ifndef CFGTYPES_H_
#define CFGTYPES_H_
#include "iron_tips.h"
#include "vars.h"

/*
 * The configuration bit map:
 * CFG_CELSIUS		- The temperature units: Celsius (1) or Fahrenheit (0)
 * CFG_BUZZER		- Is the Buzzer Enabled (1)
 * CFG_SWITCH		- Switch type: TILT (0) or REED (1)
 * CFG_AU_START		- Powering on the HAkko T12 iron at startup
 * CFG_U_CLOCKWISE	- Upper Encoder increments clockwise
 * CFG_L_CLOCKWISE	- Lower Encoder increments clockwise
 * CFG_FAST_COOLING	- Start cooling the Hot Air Gun at maximum fan speed
 * CFG_BIG_STEP		- The temperature step 1 degree (0) 5 degree (1)
 * CFG_DSPL_TYPE	- The display type IPS (true) or TFT (false)
 * CFG_SAFE_MODE	- Limit IRON high temperature
 */
typedef enum { CFG_CELSIUS = 1, CFG_BUZZER = 2, CFG_SWITCH = 4, CFG_AU_START = 8,
				CFG_U_CLOCKWISE = 16, CFG_L_CLOCKWISE = 32, CFG_FAST_COOLING = 64, CFG_BIG_STEP = 128,
				CFG_DSPL_TYPE = 256, CFG_SAFE_MODE = 512 } CFG_BIT_MASK;

typedef enum { d_t12 = 0, d_jbc = 1, d_gun = 2, d_unknown } tDevice;

typedef enum {FLASH_OK = 0, FLASH_ERROR, FLASH_NO_FILESYSTEM, FLASH_NO_DIRECTORY} FLASH_STATUS;


/* The main configuration record has the following format:
 *
 * Boost is a bit map. The upper 4 bits are boost increment temperature (n*5 Celsius), i.e.
 * 0000 - disabled
 * 0001 - +4  degrees
 * 1111 - +75 degrees
 * The lower 4 bits is the boost time ((n+1)* 20 seconds), i.e.
 * 0000 -  20 seconds
 * 0001 -  40 seconds
 * 1111 - 320 seconds
 */
typedef struct s_config RECORD;
struct s_config {
	uint16_t	crc;								// The checksum
	uint16_t	t12_temp;							// The T12 IRON preset temperature in degrees (Celsius or Fahrenheit)
	uint16_t	jbc_temp;							// The T12 IRON preset temperature in degrees (Celsius or Fahrenheit)
	uint16_t	gun_temp;							// The Hot Air Gun preset temperature in degrees (Celsius or Fahrenheit)
	uint16_t	gun_fan_speed;						// The Hot Air Gun fan speed
	RADIX		t12_tip;							// Current T12 name
	RADIX		jbc_tip;							// Current JBC tip index
	uint16_t	t12_low_temp;						// The T12 IRON low power temperature (C) or 0 if the tilt sensor is disabled
	uint8_t		t12_low_to;							// The T12 IRON low power timeout (5 seconds intervals)
	uint8_t		boost;								// Two 4-bits parameters: The boost increment temperature and boost time. See description above
	uint8_t		t12_off_timeout;					// The T12 IRON Automatic switch-off timeout in minutes [0 - 30]
	uint16_t	jbc_low_temp;						// The JBC IRON low power temperature (C) or 0 if disabled
	uint8_t		jbc_off_timeout;					// The JBC IRON Automatic switch-off timeout in minutes [0 - 30]
	uint16_t	gun_low_temp;						// The Hot Gun low power temperature (C) or 0 if disabled
	uint8_t		gun_off_timeout;					// The Hot Gun Automatic switch-off timeout in minutes [0 - 30]
	uint16_t	bit_mask;							// See CFG_BIT_MASK
	uint8_t		dspl_bright;						// The display brightness [1-100] %
	uint8_t		dspl_rotation;						// The display rotation (TFT_ROTATION_0, TFT_ROTATION_90, TFT_ROTATION_180, TFT_ROTATION_270)
	char		language[LANG_LENGTH];				// The language. LANG_LENGTH defined in vars.h
};

/* The PID custom parameters record has the following format:
 *
 */
typedef struct s_pid_params PID_PARAMS;
struct s_pid_params {
	uint16_t	crc;								// The checksum
	uint16_t	t12_Kp, t12_Ki, t12_Kd;				// The T12 IRON PID coefficients
	uint16_t	jbc_Kp, jbc_Ki, jbc_Kd;				// The JBC IRON PID coefficients
	uint16_t	gun_Kp, gun_Ki, gun_Kd;				// The Hot Air Gun PID coefficients
};

/*
 * Configuration data of each initialized tip are saved in the tipcal.dat file (16 bytes per tip record).
 * The tip configuration record has the following format:
 * 4 reference temperature points
 * tip name in RADIX encoding
 * ambient temperature when tip has been calibrating
 */
typedef struct s_tip TIP;
struct s_tip {
	uint16_t	t200, t260, t330, t400;				// The internal temperature in reference points
	RADIX		name;								// Tip name + tip calibration mask
	int8_t		ambient;							// The ambient temperature in Celsius when the tip being calibrated
	uint8_t		reserved[2];						// Not used
	uint8_t		crc;								// CRC checksum
};

// This tip structure is used to show available tips when tip is activating
typedef struct s_tip_list_item	TIP_ITEM;
struct s_tip_list_item {
	uint16_t	tip_index;							// Index of the tip in the global list read from FLASH
	RADIX		tip_name;
};


typedef enum tip_status { TIP_ACTIVE = 1, TIP_CALIBRATED = 2 } TIP_STATUS;

#endif
