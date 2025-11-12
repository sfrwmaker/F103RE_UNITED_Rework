/*
 * vars.cpp
 *
 *  2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 * 	2025 JUN 02, v1.02
 * 		Changed maximum Hot Air Gun temperature to 550 degrees
 * 	2025 NOV 11, v1.04
 * 		Added default_ambient_min and default_ambient_max
 */

#include "vars.h"

const uint16_t	int_temp_max				= 3700;			// Maximum possible temperature in internal units

const uint8_t	auto_pid_hist_length		= 16;			// The history data length of PID tuner average values
const uint8_t	ec							= 200;			// Exponential average coefficient (default value)

const uint16_t	iron_temp_minC				= 180;			// Minimum IRON calibration temperature in degrees of Celsius
const uint16_t	iron_temp_maxC_safe			= 350;			// Maximum IRON calibration temperature in degrees of Celsius in case of safe mode
const uint16_t 	iron_temp_maxC 				= 450;			// Maximum IRON calibration temperature in degrees of Celsius
const uint16_t	gun_temp_minC				= 80;			// Minimum Hot Air Gun calibration temperature in degrees of Celsius
const uint16_t 	gun_temp_maxC 				= 550;			// Maximum Hot Air Gun calibration temperature in degrees of Celsius

const uint8_t	default_ambient				= 25;			// The default ambient temperature
const uint8_t	default_ambient_min			= 15;			// The minimal value of default ambient temperature
const uint8_t	default_ambient_max			= 45;			// The maximal value of default ambient temperature
const TCHAR		nsl_cfg[9]					= {'c', 'f', 'g', '.', 'j', 's', 'o', 'n', '\0'};
const char		def_language[8]				= {'e', 'n', 'g', 'l', 'i', 's', 'h', '\0'};
const char		*standalone_msg				= "standalone";
const char		*tip_none					= "NONE";
const char		*hotgun_name				= "GUN";
