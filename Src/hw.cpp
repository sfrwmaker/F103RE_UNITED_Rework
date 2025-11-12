/*
 * hw.cpp
 *
 * 2024 NOV 16, v1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 *  2025 JAN 28
 *		Separate ambientTemp() into two routines to calculate stm32 temperature and steinhart sensor temperature inside Hakko T12 handle
 *		Save MCU internal temperature at startup to adjust internal temperature. As soon as the MCU temperature is higher than actual ambient temperature,
 *		return average value between MCU temperature and MCU temperature at startup.
 *	2025 NOV 11, v1.03
 *		Replaced the MCU temperature readings with preset value via preference menu. Used as ambient temperature when T12 handle is not connected.
 *		Modified the HW::ambientTemp() to remove calculating the MCU temperature
 *		Modified the HW::init() to remove initialization of deleted variables
 */

#include <math.h>
#include "hw.h"

CFG_STATUS HW::init(uint16_t iron_temp, uint16_t gun_temp, uint16_t ambient) {
	dspl.init();
	t_amb.length(ambient_emp_coeff);
	t_amb.reset(ambient);

	// Determine the IRON type, JBC or T12
	tDevice iron_type = d_t12;
	if (noAmbientSensor())
		iron_type = d_jbc;
	iron.init(iron_type, iron_temp);
	hotgun.init();
	hotgun.updateTemp(gun_temp);
	u_enc.start();
	l_enc.start();
	u_enc.addButton(I_ENC_B_GPIO_Port, I_ENC_B_Pin);
	l_enc.addButton(G_ENC_B_GPIO_Port, G_ENC_B_Pin);

	cfg.keepMounted(true);									// Do not unmount FLASH drive until complete initialization
	CFG_STATUS cfg_init = 	cfg.init();
	if (cfg_init == CFG_OK || cfg_init == CFG_NO_TIP) {		// Load NLS configuration data
		nls.init(&dspl);									// Setup pointer to NLS_MSG class instance to setup messages by NLS_MSG::set() method
		const char *l = cfg.getLanguage();					// Configured language name (string)
		nls.loadLanguageData(l);
		uint8_t *font = nls.font();
		dspl.setLetterFont(font);
		uint8_t r = cfg.getDsplRotation();
		dspl.rotate((tRotation)r);
	} else {
		dspl.setLetterFont(0);								// Set default font, reallocate the bitmap for 3-digits field
		dspl.rotate(TFT_ROTATION_90);
	}
	cfg.keepMounted(false);									// Now the FLASH drive can be unmount for safety data
	cfg.umount();
	PIDparam pp   		= cfg.pidParams(iron_type);			// load T12 or JBC IRON PID parameters
	iron.load(pp);
	pp					= cfg.pidParams(d_gun);				// load Hot Air Gun PID parameters
	hotgun.load(pp);
	uint16_t min_speed	= cfg.minFanSpeed();
	uint16_t max_speed	= cfg.maxFanSpeed();
	hotgun.setFanLimits(min_speed, max_speed);
	buzz.activate(cfg.isBuzzerEnabled());
	u_enc.setClockWise(cfg.isUpperEncClockWise());
	l_enc.setClockWise(cfg.isLowerEncClockWise());
	if (iron_type == d_t12)
		iron.setCheckPeriod(3);								// Start checking the T12 IRON connectivity
	return cfg_init;
}

/*
 * Return ambient temperature in Celsius
 * Caches previous result to skip expensive calculations
 */
int32_t	HW::ambientTemp(void) {
	static int32_t	raw_ambient 			= 0;			// Previous value of ambient temperature (RAW)
	static int32_t 	cached_ambient 			= 0;			// Previous value of the ambient temperature

	if (noAmbientSensor()) {								// No T12 IRON handle is connected, return default value
		return cfg.getDefAmbient();
	}
	if (abs(t_amb.read() - raw_ambient) < 25)				// About 1 Celsius degree
		return cached_ambient;
	raw_ambient = t_amb.read();

	if (raw_ambient < max_ambient_value) {					// prevent division by zero; About -30 degrees
		cached_ambient = steinhartTemp(raw_ambient);
	} else {
		cached_ambient	= default_ambient;
	}
	return cached_ambient;
}

int32_t HW::steinhartTemp(int32_t raw_ambient) {
	static const uint16_t	add_resistor	= 10000;		// The additional resistor value (10koHm)
	static const float 	  	normal_temp[2]= { 10000, 25 };	// nominal resistance and the nominal temperature
	static const uint16_t 	beta 			= 3950;     	// The beta coefficient of the thermistor (usually 3000-4000)

	// convert the value to resistance
	float resistance = 4095.0 / (float)raw_ambient - 1.0;
	resistance = (float)add_resistor / resistance;

	float steinhart = resistance / normal_temp[0];		// (R/Ro)
	steinhart = log(steinhart);							// ln(R/Ro)
	steinhart /= beta;									// 1/B * ln(R/Ro)
	steinhart += 1.0 / (normal_temp[1] + 273.15);  		// + (1/To)
	steinhart = 1.0 / steinhart;						// Invert
	steinhart -= 273.15;								// convert to Celsius
	return round(steinhart);
}
