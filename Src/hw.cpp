/*
 * hw.cpp
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 *
 */

#include <math.h>
#include "hw.h"

CFG_STATUS HW::init(uint16_t iron_temp, uint16_t gun_temp, uint16_t ambient, uint16_t vref, uint32_t t_mcu) {
	dspl.init();
	t_amb.length(ambient_emp_coeff);
	t_amb.reset(ambient);
	vrefint.length(ambient_emp_coeff);
	vrefint.reset(vref);
	t_stm32.length(ambient_emp_coeff);
	t_stm32.reset(t_mcu);

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

	cfg.keepMounted(true);									// Do not umount FLASH drive until complete initialization
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
	PIDparam pp   		= 	cfg.pidParams(iron_type);		// load T12 or JBC IRON PID parameters
	iron.load(pp);
	pp					=	cfg.pidParams(d_gun);			// load Hot Air Gun PID parameters
	hotgun.load(pp);
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
static const uint16_t	add_resistor	= 10000;			// The additional resistor value (10koHm)
static const float 	  	normal_temp[2]= { 10000, 25 };		// nominal resistance and the nominal temperature
static const uint16_t 	beta 			= 3950;     		// The beta coefficient of the thermistor (usually 3000-4000)
static const uint32_t	v_ref_int		= 12000;			// Internal voltage reference (1.2 * 10000)
static const uint32_t	v_at_25c		= 14300;			// Internal voltage at 25 degrees (1.43 * 10000)
static const uint32_t	avg_slope		= 43000;			// AVG Slope (4.3 * 10000)
static int32_t	raw_ambient 		= 0;					// Previous value of ambient temperature (RAW)
static int 		cached_ambient 		= 0;					// Previous value of the ambient temperature
static int32_t	raw_stm32			= 0;					// Previous value of MCU temperature (RAW)
static int		cached_stm32		= 0;					// Previous value of the MCU temperature

	if (noAmbientSensor()) {								// No T12 IRON handle is connected, calculate MCU internal temperature
		if (abs(t_stm32.read() - raw_stm32) < 4)			// About 1 Celsius degree
			return cached_stm32;

		// v_sense = (float)(raw_stm32 * v_ref_int/vrefint_v)
        // Temperature = (((v_at_25c - v_sense) * 1000.0) /avg_slope) + 25.0;
		raw_stm32 = t_stm32.read();
		int32_t vrefint_v = vrefint.read();
		int32_t v_sense = (raw_stm32 * v_ref_int + (vrefint_v>>1)) / vrefint_v;  // *10000
		cached_stm32 = ((v_at_25c - v_sense) * 1000 + (avg_slope>>1)) / avg_slope + 25;
		return cached_stm32;
	}
	if (abs(t_amb.read() - raw_ambient) < 25)				// About 1 Celsius degree
		return cached_ambient;
	raw_ambient = t_amb.read();

	if (raw_ambient < max_ambient_value) {					// prevent division by zero; About -30 degrees
		// convert the value to resistance
		float resistance = 4095.0 / (float)raw_ambient - 1.0;
		resistance = (float)add_resistor / resistance;

		float steinhart = resistance / normal_temp[0];		// (R/Ro)
		steinhart = log(steinhart);							// ln(R/Ro)
		steinhart /= beta;									// 1/B * ln(R/Ro)
		steinhart += 1.0 / (normal_temp[1] + 273.15);  		// + (1/To)
		steinhart = 1.0 / steinhart;						// Invert
		steinhart -= 273.15;								// convert to Celsius
		cached_ambient	= round(steinhart);
	} else {
		cached_ambient	= default_ambient;
	}
	return cached_ambient;
}
