/*
 * config.cpp
 *
 *  Created on: 15 aug. 2019.
 *      Author: Alex
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 *
 */

#include <stdlib.h>
#include "config.h"
#include "tools.h"
#include "vars.h"

// Initialize the configuration. Find the actual record in the EEPROM.
CFG_STATUS CFG::init(void) {
	FLASH_STATUS status = W25Q::init();
	if (status == FLASH_OK) {
		loadGlobalTipList();
		uint8_t tips_loaded = 0;
		if (tips.total() > 0) {								// The memory is allocated for the tip table
			tips_loaded = buildTipTable();
		}

		bool cfg_ok = loadRecord(&a_cfg);
		if (cfg_ok) {
			correctConfig(&a_cfg);
		} else {
			setDefaults();
			a_cfg.t12_tip = nearActiveTip(a_cfg.t12_tip);
			a_cfg.jbc_tip = nearActiveTip(a_cfg.jbc_tip);
		}

		if (!loadPIDparams(&pid))
			setPIDdefaults();

		selectTip(tips.radix(0));							// Load Hot Air Gun calibration data
		selectTip(a_cfg.t12_tip);							// Load T12 tip configuration data into a_tip variable
		selectTip(a_cfg.jbc_tip);							// Load JBC tip configuration data into a_tip variable
		CFG_CORE::syncConfig();								// Update spare configuration
		if (tips_loaded > 0) {
			if (!cfg_ok) {									// The current configuration was illegal
				saveRecord(&a_cfg);
			}
			return CFG_OK;
		} else {
			if (tips.total() > 1)
				return CFG_NO_TIP;
			else
				return CFG_NO_TIP_LIST;
		}
	} else {												// FLASH is not writable or is not ready
		setDefaults();
		setPIDdefaults();
		resetTipCalibration(d_gun);
		selectTip(a_cfg.t12_tip);							// 0-th tip is a hot AIr Gun
		selectTip(a_cfg.jbc_tip);
		CFG_CORE::syncConfig();
	}
	if (status == FLASH_ERROR) {
		return CFG_READ_ERROR;
	} else if (status == FLASH_NO_FILESYSTEM) {
		return CFG_NO_FILESYSTEM;
	}
	return CFG_OK;
}

bool CFG::reloadTips(void) {
	if (tips.total() > 0) {									// Tips table allocated
		buildTipTable();
		return true;
	}
	return false;
}

void CFG::correctConfig(RECORD *cfg) {
	uint16_t t12_tempC = cfg->t12_temp;
	uint16_t jbc_tempC = cfg->jbc_temp;
	uint16_t gun_tempC = cfg->gun_temp;
	if (!(cfg->bit_mask & CFG_CELSIUS)) {
		t12_tempC	= fahrenheitToCelsius(t12_tempC);
		jbc_tempC	= fahrenheitToCelsius(jbc_tempC);
		gun_tempC	= fahrenheitToCelsius(gun_tempC);
	}
	t12_tempC	= constrain(t12_tempC, iron_temp_minC, iron_temp_maxC);
	jbc_tempC	= constrain(jbc_tempC, iron_temp_minC, iron_temp_maxC);
	gun_tempC	= constrain(gun_tempC,  gun_temp_minC,  gun_temp_maxC);
	if (!(cfg->bit_mask & CFG_CELSIUS)) {
		t12_tempC	= celsiusToFahrenheit(t12_tempC);
		jbc_tempC	= celsiusToFahrenheit(jbc_tempC);
		gun_tempC 	= celsiusToFahrenheit(gun_tempC);
	}
	cfg->t12_temp	= t12_tempC;
	cfg->jbc_temp	= jbc_tempC;
	cfg->gun_temp	= gun_tempC;
	if (cfg->t12_off_timeout > 30)
		cfg->t12_off_timeout = 30;
	if (cfg->jbc_off_timeout > 30)
		cfg->jbc_off_timeout = 30;
	cfg->t12_tip = nearActiveTip(cfg->t12_tip);
	cfg->jbc_tip = nearActiveTip(cfg->jbc_tip);
	cfg->dspl_bright = constrain(cfg->dspl_bright, 10, 255);
}

// Load calibration data of the tip from FLASH drive. If the tip is not calibrated, initialize the calibration data with the default values
bool CFG::selectTip(RADIX& tip_name) {
	int16_t tip_global = tips.index(tip_name);
	if (tip_global < 0) return false;						// The tip is not found in the global list

	bool result = true;
	uint8_t tip_index = tips.tipCalibrationIndex(tip_global);
	tDevice dev_type = hardwareType(tip_name);
	if (tip_index == NO_TIP_CHUNK) {
		TIP_CFG::resetTipCalibration(dev_type);
		return false;
	}
	TIP tip;
	if (loadTipData(&tip, tip_index) != TIP_OK) {
		TIP_CFG::resetTipCalibration(dev_type);
		result = false;
	} else {
		if (!(tip.name.isCalibrated())) {					// Tip is not calibrated, load default configuration
			TIP_CFG::resetTipCalibration(dev_type);
		} else if (!isValidTipConfig(&tip)) {
			TIP_CFG::resetTipCalibration(dev_type);
		} else {											// Tip configuration record is completely correct
			TIP_CFG::load(tip, dev_type);
		}
	}
	return result;
}

// Change the current tip. Save configuration to the FLASH
void CFG::changeTip(RADIX& tip_name) {
	tDevice dev_type = hardwareType(tip_name);
	if (selectTip(tip_name)) {
		if (dev_type == d_t12) {
			a_cfg.t12_tip = tip_name;
		} else if (dev_type == d_jbc) {
			a_cfg.jbc_tip = tip_name;
		}
		saveConfig();
	}
}

uint16_t CFG::currentTipIndex(tDevice dev) {
	RADIX tip_name = currentTip(dev);
	int16_t index = tips.index(tip_name);
	if (index < 0) index = 0;
	return index;
}

RADIX& CFG::currentTip(tDevice dev) {
	if (dev == d_gun)
		return tips.radix(0);
	if (dev == d_t12)
		return a_cfg.t12_tip;
	return a_cfg.jbc_tip;
}

/*
 * Translate the internal temperature of the IRON or Hot Air Gun to the human readable units (Celsius or Fahrenheit)
 * Parameters:
 * temp 		- Device temperature in internal units
 * ambient		- The ambient temperature
 * dev			- Device: T12, JBC or Hot Gun
 */
uint16_t CFG::tempToHuman(uint16_t temp, int16_t ambient, tDevice dev) {
	uint16_t tempH = TIP_CFG::tempCelsius(temp, ambient, dev);
	if (!CFG_CORE::isCelsius())
		tempH = celsiusToFahrenheit(tempH);
	return tempH;
}

// Translate the temperature from human readable units (Celsius or Fahrenheit) to the internal units
uint16_t CFG::humanToTemp(uint16_t t, int16_t ambient, tDevice dev, bool no_lower_limit) {
	int d = ambient - TIP_CFG::ambientTemp(dev);
	uint16_t t200	= referenceTemp(0, dev) + d;
	uint16_t t400	= referenceTemp(3, dev) + d;
	uint16_t tmin	= tempMin(dev, true);					// The minimal temperature, Celsius
	uint16_t tmax	= tempMax(dev, true);					// The maximal temperature, Celsius
	if (no_lower_limit) tmin = 100;
	if (!CFG_CORE::isCelsius()) {
		t200 = celsiusToFahrenheit(t200);
		t400 = celsiusToFahrenheit(t400);
		tmin = celsiusToFahrenheit(tmin);
		tmax = celsiusToFahrenheit(tmax);
	}
	t = constrain(t, tmin, tmax);

	uint16_t left 	= 0;
	uint16_t right 	= int_temp_max;
	uint16_t temp = emap(t, t200, t400, TIP_CFG::calibration(0, dev), TIP_CFG::calibration(3, dev));

	if (temp > (left+right)/ 2) {
		temp -= (right-left) / 4;
	} else {
		temp += (right-left) / 4;
	}

	for (uint8_t i = 0; i < 20; ++i) {
		uint16_t tempH = tempToHuman(temp, ambient, dev);
		if (tempH == t) {
			return temp;
		}
		uint16_t new_temp;
		if (tempH < t) {
			left = temp;
			 new_temp = (left+right)/2;
			if (new_temp == temp)
				new_temp = temp + 1;
		} else {
			right = temp;
			new_temp = (left+right)/2;
			if (new_temp == temp)
				new_temp = temp - 1;
		}
		temp = new_temp;
	}
	return temp;
}

// Build the complete tip name (including "T12-" prefix)
std::string CFG::tipName(tDevice dev) {
	RADIX& tip_name = currentTip(dev);
	return tip_name.tipName();
}

// Save current configuration to the flash
void CFG::saveConfig(void) {
	if (CFG_CORE::areConfigsIdentical())
		return;
	saveRecord(&a_cfg);										// calculates CRC and changes ID
	CFG_CORE::syncConfig();
}

void CFG::savePID(PIDparam &pp, tDevice dev) {
	if (dev == d_t12) {
		pid.t12_Kp	= pp.Kp;
		pid.t12_Ki	= pp.Ki;
		pid.t12_Kd	= pp.Kd;
	} else if (dev == d_gun){
		pid.gun_Kp	= pp.Kp;
		pid.gun_Ki	= pp.Ki;
		pid.gun_Kd	= pp.Kd;
	} else {
		pid.jbc_Kp	= pp.Kp;
		pid.jbc_Ki	= pp.Ki;
		pid.jbc_Kd	= pp.Kd;
	}
	savePIDparams(&pid);
}

// Save new IRON tip calibration data to the FLASH only. Do not change active configuration
bool CFG::saveTipCalibtarion(tDevice dev, uint16_t temp[4], uint8_t mask, int8_t ambient) {
	TIP tip;
	tip.t200		= temp[0];
	tip.t260		= temp[1];
	tip.t330		= temp[2];
	tip.t400		= temp[3];
	tip.ambient		= ambient;
	tip.name		= currentTip(dev);
	if (isValidTipConfig(&tip)) {
		tip.name.setCalibMask(mask);
		int16_t tip_index = saveTipData(&tip);
		if (tip_index >= 0) {
			tips.applyCalibtationIndex(tip.name, tip_index);
			return true;
		}
	}
	tip.name.clearCalibrated();								// The tip is not calibrated
	return false;
}

bool CFG::isTipCalibrated(tDevice dev) {
	RADIX& tip_name = currentTip(dev);
	return tip_name.isCalibrated();

}

// Toggle (activate/deactivate) tip activation flag. Do not change active tip configuration
bool CFG::toggleTipActivation(uint16_t global_tip_index) {
	if (tips.total() < 2)	return false;					// Not tip list loaded
	bool ret		= false;
	TIP tip;
	int16_t calib_index = tips.tipCalibrationIndex(global_tip_index); // TIP index in tipcal.dat file
	if (calib_index == NO_TIP_CHUNK) {						// This tip data is not in the tipcal.dat file, it was not active!
		RADIX r = tips.radix(global_tip_index);
		if (!r.isEmpty()) {
			r.setActivated();
			tip.name = r;									// Initialize tip name
			defaultCalibration(&tip);						// Initialize default tip calibration
			ret			= true;
		}
	} else {												// Tip configuration data exists in the EEPROM
		if (loadTipData(&tip, calib_index, true) == TIP_OK) {
			if (tip.name.isActivated())						// Toggle tip activation
				tip.name.clearActivated();
			else
				tip.name.setActivated();
			ret = true;
		}
	}
	if (!ret) return false;

	calib_index = saveTipData(&tip, true);
	if (calib_index >= 0) {
		tips.applyCalibtationIndex(tip.name, calib_index);
		return true;
	}
	return false;
}

 /*
  * Build the tip list starting from the current tip name;
  * In tip activation mode
  * 	active_only is false
  * 	manual_change is false
  * 	dev_type is d_unknown
  * In automatic tip selection mode
  * 	active_only is true
  * 	manual_change is false
  * 	dev_type is d_t12 or d_jbc
  * In manual selection mode
  * 	active_only is true
  * 	manual_change is true
  * 	dev_type is d_t12 or d_jbc
  */
uint8_t	CFG::tipList(uint8_t current, TIP_ITEM list[], uint8_t list_len, bool active_only, bool manual_change, tDevice dev_type) {
	if (tips.total() == 0) {								// If tip_table is not initialized, return empty list
		for (uint8_t tip_index = 0; tip_index < list_len; ++tip_index) {
			list[tip_index].tip_name.initEmpty();
		}
		return 0;
	}

	// Seek several (previous) tips backward
	int16_t tip_index = current-1;
	uint8_t previous = 3;
	for (; tip_index > 0; --tip_index) {
		if (!active_only || (tips.radix(tip_index).isActivated())) {
			if (--previous == 0)
				break;
		}
	}
	uint8_t loaded = 0;
	if (tip_index < 0) tip_index = 0;						// Ensure the tip index is not negative inside next loop, because the tip_table will be read
	for (; tip_index < tips.total(); ++tip_index) {
		if (tip_index == 0) continue;						// Skip Hot Air Gun 'tip'
		RADIX r = tips.radix(tip_index);					// The tip name
		if (active_only && !(r.isActivated()))
			continue;										// Skip not activated tip (if we should draw activated tips only)
		if (manual_change && dev_type != hardwareType(r))
			continue;										// Skip tip of wrong hardware type
		if (!manual_change && dev_type == d_t12 && r.type() == TIP_NONE)
			continue;										// Skip not native T12 tip
		list[loaded].tip_index	= tip_index;
		list[loaded].tip_name   = r;
		++loaded;
		if (loaded >= list_len)	break;
	}
	for (uint8_t tip_index = loaded; tip_index < list_len; ++tip_index) {
		list[tip_index].tip_name.initEmpty();				// Clear rest of the list
	}
	return loaded;
}

// Check the current tip is active. Return nearest active tip or 1 if no one tip has been activated
RADIX CFG::nearActiveTip(RADIX& current_tip) {
	int16_t 	tip_index	= tips.index(current_tip);
	TIP_TYPE_t	tip_type	= current_tip.type();
	RADIX		res			= tips.radix(tip_index);		// Existing tip or ZERO
	if (tip_index < 0) {
		uint16_t existing = 65535;							// For sure non-existing tip index
		for (uint16_t i = 0; i < tips.total(); ++i) {
			RADIX r = tips.radix(i);
			if (tip_type == r.type()) {
				existing = i;
				if (r.isActivated())
					return r;
			}
		}
		res = tips.radix(existing);
		if (res.isEmpty()) {								// Not found tip of the type specified in the global tip list
			res.init(tip_type, tip_none, strlen(tip_none)); // Generate tip name; tip_none defined in vars.h
		}
		return res;
	} else 	if (tip_index == 0)	{							// The Hot Air Gun has index equal to zero, the only tip available of this type
		return tips.radix(0);								// The Hot Air Gun is always exist in the tip table
	}

	// Main case: the tip has been found in the global list
	if (res.isActivated())
		return res;											// Current tip is activated, so use it
	uint16_t low_index = tip_index -1;
	for ( ; low_index > 0; --low_index) {
		RADIX r = tips.radix(low_index);
		if (r.isActivated())
			break;
	}
	uint16_t upp_index = tip_index + 1;
	for ( ; upp_index < tips.total(); ++upp_index) {
		RADIX r = tips.radix(upp_index);
		if (r.isActivated())
			break;
	}
	if (low_index == 0 && upp_index >= tips.total()) {		// No near tip the specified type found
		return res;											// Despite the current tip is not active
	}
	if (low_index == 0) {
		res = tips.radix(upp_index);
	} else if (upp_index >= tips.total()) {
		res = tips.radix(low_index);
	} else {												// Both side neighbor found
		if (abs(tip_index - low_index) < abs(upp_index - tip_index))
			res = tips.radix(low_index);
		else
			res = tips.radix(upp_index);
	}
	return res;
}

// Initialize the configuration area. Save default configuration to the FLASH
void CFG::initConfig(void) {
	if (clearConfig()) {									// Format FLASH
		setDefaults();										// Create default configuration
		saveRecord(&a_cfg);									// Save default config
		clearAllTipsCalibration();							// Clear tip calibration data
	}
}

bool CFG::clearAllTipsCalibration(void) {
	tips.clearAllCalibration();
	return clearTips();
}

void CFG::applyTipCalibtarion(uint16_t temp[4], int8_t ambient, tDevice dev, bool calibrated) {
	changeTipCalibtarion(temp, ambient, dev);
	RADIX& tip_name = currentTip(dev);
	tip_name.setActivated();
	if (calibrated)
		tip_name.setCalibrated();
}

uint16_t CFG_CORE::tempMin(tDevice dev, bool force_celsius) {
	uint16_t t = (dev == d_gun)?gun_temp_minC:iron_temp_minC;
	if (!force_celsius && !isCelsius()) {					// Convert to Fahrenheit
		t = celsiusToFahrenheit(t);
		t -= t % 10;										// Round left to be multiplied by 10
	}
	return t;
}

uint16_t CFG_CORE::tempMax(tDevice dev, bool force_celsius) {
	return tempMax(dev, force_celsius || isCelsius(), isSafeIronMode());
}

uint16_t CFG_CORE::tempMax(tDevice dev, bool celsius, bool safe_iron_mode) {
	uint16_t t = gun_temp_maxC;
	if (safe_iron_mode)
		t = iron_temp_maxC_safe;
	else
		t = iron_temp_maxC;
	if (!celsius) {											// Convert to Fahrenheit
		t = celsiusToFahrenheit(t);
		t += 10 - t % 10;									// Round right to be multiplied by 10
	}
	return t;
}

/*
 * Builds the tip configuration table: reads the tip calibration data fron tipcal.dat
 * and save index of calibrated tip into tip_table
 */
uint8_t CFG::buildTipTable(void) {
	TIP		tmp_tip;
//	RADIX	r;
	uint8_t	loaded 	= 0;
	for (int16_t i = 0; i < tips.total(); ++i) {			// Limit this loop by whole TIP list for reliability
		TIP_IO_STATUS ts = loadTipData(&tmp_tip, i, true);
		if (ts == TIP_OK) {
			if (!isValidTipConfig(&tmp_tip))
				tmp_tip.name.clearCalibrated();
			if (tips.applyCalibtationIndex(tmp_tip.name, i))
				++loaded;
		} else if (ts == TIP_IO) {
			break;
		}
	}
	W25Q::umount();
	return loaded;
}

uint16_t CFG::loadGlobalTipList(void) {
	uint8_t tip_name[16];
	uint16_t tip_count = 0;
	uint8_t br = 0;
	RADIX r;
	while ((br = tipListReadNextItem((const char *)tip_name, 16))) {
		if (r.init((const char *)tip_name, br))
			++tip_count;
	}
	tipListEnd();
	++tip_count;											// Extra tip is for Hot Air Gun
	if (tips.create(tip_count)) {							// Try to allocate memory for the tip list
		r.init(TIP_HOTGUN, hotgun_name, strlen(hotgun_name)); // hotgun_name defined in vars.h
		tips.initTip(0, r);
		for (uint16_t tip = 1; tip < tip_count;) {
			br = tipListReadNextItem((const char *)tip_name, 16);
			if (br == 0) break;
			if (r.init((const char *)tip_name, br)) {
				if (!tips.initTip(tip, r))
					break;
				++tip;
			}
		}
		tipListEnd();
	}
	return tip_count;
}

// Compare two configurations
bool CFG_CORE::areConfigsIdentical(void) {
	if (a_cfg.t12_temp 			!= s_cfg.t12_temp) 			return false;
	if (a_cfg.jbc_temp 			!= s_cfg.jbc_temp) 			return false;
	if (a_cfg.gun_temp 			!= s_cfg.gun_temp) 			return false;
	if (a_cfg.gun_fan_speed 	!= s_cfg.gun_fan_speed)		return false;
	if (a_cfg.t12_low_temp		!= s_cfg.t12_low_temp)		return false;
	if (a_cfg.t12_low_to		!= s_cfg.t12_low_to)		return false;
	if (a_cfg.t12_off_timeout 	!= s_cfg.t12_off_timeout)	return false;
	if (a_cfg.jbc_low_temp		!= s_cfg.jbc_low_temp)		return false;
	if (a_cfg.jbc_off_timeout 	!= s_cfg.jbc_off_timeout)	return false;
	if (a_cfg.bit_mask			!= s_cfg.bit_mask)			return false;
	if (a_cfg.boost				!= s_cfg.boost)				return false;
	if (a_cfg.dspl_bright		!= s_cfg.dspl_bright)		return false;
	if (a_cfg.gun_low_temp		!= s_cfg.gun_low_temp)		return false;
	if (a_cfg.gun_off_timeout	!= s_cfg.gun_off_timeout)	return false;
	if (!a_cfg.t12_tip.match(s_cfg.t12_tip))				return false;
	if (!a_cfg.jbc_tip.match(s_cfg.jbc_tip))				return false;
	if (strncmp(a_cfg.language, s_cfg.language, LANG_LENGTH)  != 0)	return false;
	return true;
};

//---------------------- CORE_CFG class functions --------------------------------
void CFG_CORE::setDefaults(void) {
	a_cfg.t12_temp			= 235;
	a_cfg.jbc_temp			= 235;
	a_cfg.gun_temp			= 200;
	a_cfg.gun_fan_speed		= 1200;
	a_cfg.t12_off_timeout	= 5;
	a_cfg.t12_low_temp		= 180;
	a_cfg.t12_low_to		= 5;
	a_cfg.jbc_low_temp		= 180;
	a_cfg.jbc_off_timeout	= 5;							// Minutes, 0 means switch the IRON immediately
	a_cfg.bit_mask			= CFG_CELSIUS | CFG_BUZZER | CFG_U_CLOCKWISE | CFG_L_CLOCKWISE | CFG_BIG_STEP;
	a_cfg.boost				= 80;
	a_cfg.dspl_bright		= 128;
	a_cfg.dspl_rotation		=  1;							// TFT_ROTATION_90;
	a_cfg.gun_off_timeout	= 0;
	a_cfg.gun_low_temp		= 180;
	strncpy(a_cfg.language, def_language, LANG_LENGTH);
	a_cfg.t12_tip.init(TIP_T12, tip_none, strlen(tip_none)); // tip_none defined in vars.h
	a_cfg.jbc_tip.init(TIP_JBC, tip_none, strlen(tip_none));
}

void CFG_CORE::setPIDdefaults(void) {
	pid.t12_Kp			= 2300;	//6217;
	pid.t12_Ki			=   50;	// 37;
	pid.t12_Kd			=  735;	// 2960;
	pid.jbc_Kp			= 1479;
	pid.jbc_Ki			=   59;
	pid.jbc_Kd			=  507;
	pid.gun_Kp			=  200;
	pid.gun_Ki			=   64;
	pid.gun_Kd			=  195;
};

// PID parameters: Kp, Ki, Kd for smooth work, i.e. tip calibration
PIDparam CFG_CORE::pidParamsSmooth(tDevice dev) {
	if (dev == d_t12) {
		return PIDparam(575, 10, 200);
	} else if (dev == d_gun) {
		return PIDparam(150, 64, 50);
	} else {
		return PIDparam(500, 3, 1700);
	}
}

uint8_t	CFG_CORE::getOffTimeout(tDevice dev) {
	if (dev == d_jbc)
		return a_cfg.jbc_off_timeout;
	else if (dev == d_t12)
		return a_cfg.t12_off_timeout;
	else
		return a_cfg.gun_off_timeout;
}

uint16_t CFG_CORE::getLowTemp(tDevice dev)	{
	if (dev == d_jbc)
		return a_cfg.jbc_low_temp;
	else if (dev == d_t12)
		return a_cfg.t12_low_temp;
	else
		return a_cfg.gun_low_temp;
}

uint16_t CFG_CORE::tempPresetHuman(tDevice dev) {
	if (dev == d_gun) {
		return a_cfg.gun_temp;
	} else if (dev == d_jbc) {
		return a_cfg.jbc_temp;
	}
	return a_cfg.t12_temp;
}

const char *CFG_CORE::getLanguage(void) {
	if (a_cfg.language[0] == '\0')
		strncpy(a_cfg.language, def_language, LANG_LENGTH);
	return a_cfg.language;
}

// Apply main configuration parameters: automatic off timeout, buzzer and temperature units
void CFG_CORE::setup(bool buzzer, bool celsius, bool big_temp_step, bool i_enc, bool g_enc, bool ips_display, bool safe_iron_mode, uint8_t bright) {
	bool cfg_celsius		= a_cfg.bit_mask & CFG_CELSIUS;
	if (cfg_celsius	!= celsius) {							// When we change units, the temperature should be converted
		if (celsius) {										// Translate preset temp. from Fahrenheit to Celsius
			a_cfg.t12_temp	= fahrenheitToCelsius(a_cfg.t12_temp);
			a_cfg.jbc_temp	= fahrenheitToCelsius(a_cfg.jbc_temp);
			a_cfg.gun_temp	= fahrenheitToCelsius(a_cfg.gun_temp);
		} else {											// Translate preset temp. from Celsius to Fahrenheit
			a_cfg.t12_temp	= celsiusToFahrenheit(a_cfg.t12_temp);
			a_cfg.jbc_temp	= celsiusToFahrenheit(a_cfg.jbc_temp);
			a_cfg.gun_temp	= celsiusToFahrenheit(a_cfg.gun_temp);
		}
	}
	a_cfg.bit_mask	&=  CFG_SWITCH | CFG_AU_START;			// Preserve these bits
	if (celsius)		a_cfg.bit_mask |= CFG_CELSIUS;
	if (buzzer)			a_cfg.bit_mask |= CFG_BUZZER;
	if (big_temp_step)	a_cfg.bit_mask |= CFG_BIG_STEP;
	if (i_enc)			a_cfg.bit_mask |= CFG_U_CLOCKWISE;
	if (g_enc)			a_cfg.bit_mask |= CFG_L_CLOCKWISE;
	if (ips_display)	a_cfg.bit_mask |= CFG_DSPL_TYPE;
	if (safe_iron_mode)	a_cfg.bit_mask |= CFG_SAFE_MODE;
	a_cfg.dspl_bright	= constrain(bright, 1, 100);
	if (safe_iron_mode) {									// Limit the iron preset temperature
		uint16_t t_max = tempMax(d_t12);
		if (a_cfg.t12_temp > t_max) a_cfg.t12_temp = t_max;
		if (a_cfg.jbc_temp > t_max) a_cfg.jbc_temp = t_max;
	}
}

void CFG_CORE::setupT12(bool reed, bool auto_start, uint8_t off_timeout, uint16_t low_temp, uint8_t low_to, uint8_t delta_temp, uint16_t duration) {
	a_cfg.t12_off_timeout	= off_timeout;
	a_cfg.t12_low_temp		= low_temp;
	a_cfg.t12_low_to		= low_to;
	a_cfg.bit_mask	&= ~(CFG_SWITCH | CFG_AU_START);
	if (reed)			a_cfg.bit_mask |= CFG_SWITCH;
	if (auto_start) 	a_cfg.bit_mask |= CFG_AU_START;
	if (delta_temp > 75) delta_temp = 75;
	if (duration > 320)	duration = 320;
	if (duration < 5)   duration = 5;
	delta_temp += 4;
	delta_temp /= 5;
	a_cfg.boost = delta_temp << 4;
	a_cfg.boost &= 0xF0;
	a_cfg.boost |= ((duration-1)/20) & 0xF;
}

void CFG_CORE::setupJBC(uint8_t off_timeout, uint16_t stby_temp) {
	a_cfg.jbc_low_temp 		= stby_temp;
	a_cfg.jbc_off_timeout	= constrain(off_timeout, 0, 30);
}

void CFG_CORE::setupGUN(bool fast_gun_chill, uint8_t stby_timeout, uint16_t stby_temp) {
	if (fast_gun_chill) {
		a_cfg.bit_mask		|= CFG_FAST_COOLING;
	} else {
		a_cfg.bit_mask		&= ~CFG_FAST_COOLING;
	}
	a_cfg.gun_off_timeout	= stby_timeout;
	a_cfg.gun_low_temp		= stby_temp;
}

void CFG_CORE::savePresetTempHuman(uint16_t temp_set, tDevice dev_type) {
	if (dev_type == d_t12)
		a_cfg.t12_temp = temp_set;
	else if (dev_type == d_jbc)
		a_cfg.jbc_temp = temp_set;
}

void CFG_CORE::saveGunPreset(uint16_t temp_set, uint16_t fan) {
	a_cfg.gun_temp 		= temp_set;
	a_cfg.gun_fan_speed	= fan;
}

void CFG_CORE::syncConfig(void)	{
	memcpy(&s_cfg, &a_cfg, sizeof(RECORD));
}

void CFG_CORE::restoreConfig(void) {
	memcpy(&a_cfg, &s_cfg, sizeof(RECORD));					// restore configuration from spare copy
}

/*
 * Boost is a bit map. The upper 4 bits are boost increment temperature (n*5 Celsius), i.e.
 * 0000 - disabled
 * 0001 - +5  degrees
 * 1111 - +75 degrees
 * The lower 4 bits is the boost time ((n+1)* 5 seconds), i.e.
 * 0000 -  5 seconds
 * 0001 - 10 seconds
 * 1111 - 80 seconds
 */
uint8_t	CFG_CORE::boostTemp(void){
	uint8_t t = a_cfg.boost >> 4;
	return t * 5;
}

uint16_t CFG_CORE::boostDuration(void) {
	uint16_t d = a_cfg.boost & 0xF;
	return (d+1)*20;
}

// Save boost parameters to the current configuration
void CFG_CORE::saveBoost(uint8_t temp, uint16_t duration) {
	if (temp > 75)		temp = 75;
	if (duration > 320)	duration = 320;
	if (duration < 5)   duration = 5;
	temp += 4;
	temp /= 5;
	a_cfg.boost = temp << 4;
	a_cfg.boost &= 0xF0;
	a_cfg.boost |= ((duration-1)/20) & 0xF;
}

// PID parameters: Kp, Ki, Kd
PIDparam CFG_CORE::pidParams(tDevice dev) {
	if (dev == d_t12) {
		return PIDparam(pid.t12_Kp, pid.t12_Ki, pid.t12_Kd);
	} else if (dev == d_gun) {
		return PIDparam(pid.gun_Kp, pid.gun_Ki, pid.gun_Kd);
	} else {
		return PIDparam(pid.jbc_Kp, pid.jbc_Ki, pid.jbc_Kd);
	}
}

//---------------------- CORE_CFG class functions --------------------------------
void TIP_CFG::load(const TIP& ltip, tDevice dev) {
	uint8_t i = uint8_t(dev);
	if (i >= 3) return;
	tip[i].calibration[0]	= ltip.t200;
	tip[i].calibration[1]	= ltip.t260;
	tip[i].calibration[2]	= ltip.t330;
	tip[i].calibration[3]	= ltip.t400;
	tip[i].ambient			= ltip.ambient;
}

void TIP_CFG::dump(TIP* ltip, tDevice dev) {
	uint8_t i = uint8_t(dev);
	ltip->t200		= tip[i].calibration[0];
	ltip->t260		= tip[i].calibration[1];
	ltip->t330		= tip[i].calibration[2];
	ltip->t400		= tip[i].calibration[3];
	ltip->ambient	= tip[i].ambient;
}

int8_t TIP_CFG::ambientTemp(tDevice dev) {
	uint8_t i = uint8_t(dev);
	if (i > 2) i = 0;
	return tip[i].ambient;
}

uint16_t TIP_CFG::calibration(uint8_t index, tDevice dev) {
	if (index >= 4)
		return 0;
	uint8_t i = uint8_t(dev);
	return tip[i].calibration[index];
}

// Apply new IRON tip calibration data to the current configuration
void TIP_CFG::changeTipCalibtarion(uint16_t temp[4], int8_t ambient, tDevice dev) {
	uint8_t i = uint8_t(dev);
	for (uint8_t j = 0; j < 4; ++j)
		tip[i].calibration[j]	= temp[j];
	if (tip[i].calibration[3] > int_temp_max) tip[i].calibration[3] = int_temp_max;
	tip[i].ambient	= ambient;
}

uint16_t TIP_CFG::referenceTemp(uint8_t index, tDevice dev) {
	if (dev == d_gun)
		return temp_ref_gun[index];
	else
		return temp_ref_iron[index];
}

// Translate the internal temperature of the IRON or Hot Air Gun to Celsius
uint16_t TIP_CFG::tempCelsius(uint16_t temp, int16_t ambient, tDevice dev) {
	uint8_t i 		= uint8_t(dev);								// Select appropriate calibration tip or gun
	int16_t tempH 	= 0;
	if (i > 2) return 0;

	// The temperature difference between current ambient temperature and ambient temperature during tip calibration
	int d = ambient - tip[i].ambient;
	if (temp < tip[i].calibration[0]) {							// less than first calibration point
	    tempH = map(temp, 0, tip[i].calibration[0], ambient, referenceTemp(0, dev)+d);
	} else {
		if (temp <= tip[i].calibration[3]) {					// Inside calibration interval
			for (uint8_t j = 1; j < 4; ++j) {
				if (temp < tip[i].calibration[j]) {
					tempH = map(temp, tip[i].calibration[j-1], tip[i].calibration[j],
							referenceTemp(j-1, dev)+d, referenceTemp(j, dev)+d);
					break;
				}
			}
		} else {												// Greater than maximum
			if (tip[i].calibration[1] < tip[i].calibration[3]) { // If tip calibrated correctly
				tempH = emap(temp, tip[i].calibration[1], tip[i].calibration[3],
					referenceTemp(1, dev)+d, referenceTemp(3, dev)+d);
			} else {											// Perhaps, the tip calibration process
				tempH = emap(temp, tip[i].calibration[1], int_temp_max,
							referenceTemp(1, dev)+d, referenceTemp(3, dev)+d);
			}
		}
	}
	tempH = constrain(tempH, ambient, 999);
	return tempH;
}

// Return the reference temperature points of the IRON tip calibration
void TIP_CFG::getTipCalibtarion(uint16_t temp[4], tDevice dev) {
	uint8_t i = uint8_t(dev);
	for (uint8_t j = 0; j < 4; ++j)
		temp[j]	= tip[i].calibration[j];
}

// Apply default calibration parameters of the tip; Prevent overheating of the tip
void TIP_CFG::resetTipCalibration(tDevice dev) {
	uint8_t dev_indx = uint8_t(dev);
	for (uint8_t i = 0; i < 4; ++i)
		tip[dev_indx].calibration[i] = calib_default[i];
	tip[dev_indx].ambient	= default_ambient;					// default_ambient defined in vars.cpp
}

void TIP_CFG::defaultCalibration(TIP *tip) {
	uint8_t i = 0;
	tip->t200				= calib_default[i++];
	tip->t260				= calib_default[i++];
	tip->t330				= calib_default[i++];
	tip->t400				= calib_default[i];
}

tDevice TIP_CFG::hardwareType(RADIX &tip_name) {
	TIP_TYPE_t tip_type = tip_name.type();
	switch (tip_type) {
		case TIP_HOTGUN:
			return d_gun;
		case TIP_JBC:
		case TIP_C245:
			return d_jbc;
		case TIP_NONE:
		case TIP_T12:
		case TIP_N1:
			return d_t12;
		case TIP_INVALID:
		default:
			break;
	}
	return d_unknown;
}

bool TIP_CFG::isValidTipConfig(TIP *tip) {
	if (tip->t200 >= tip->t260 || (tip->t260 - tip->t200) < min_temp_diff) return false;
	if (tip->t260 >= tip->t330 || (tip->t330 - tip->t260) < min_temp_diff) return false;
	if (tip->t330 >= tip->t400 || (tip->t400 - tip->t330) < min_temp_diff) return false;
	return true;
}
