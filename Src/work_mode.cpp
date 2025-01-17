/*
 * work_mode.cpp
 *
 * 2024 NOV 16, v1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 *
 */

#include "work_mode.h"
#include "core.h"

//-------------------- The iron main working mode, keep the temperature ----------
// Do initialize the IRONs and Hot Air Gun preset temperature when the device is turned on
void MWORK::init(void) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	HOTGUN*	pHG		= &pCore->hotgun;

	ambient					= pCore->ambientTemp();
	uint16_t 	fan			= pCFG->gunFanPreset();
	pHG->setFan(fan);										// Setup the Hot Air Gun fan speed to be able to use the pHG->presetFanPcnt() (see below)
	DASH::init();											// Initializes the iron_dev type
	// Initialize devices with a preset temperature
	tDevice  iron_dev	= pCore->iron.deviceType();
	uint16_t temp		= pCFG->tempPresetHuman(iron_dev);
	uint16_t temp_i		= pCFG->humanToTemp(temp, ambient, iron_dev);
	pCore->iron.setTemp(temp_i);
	temp				= pCFG->tempPresetHuman(d_gun);
	temp_i				= pCFG->humanToTemp(temp, ambient, d_gun);
	pCore->hotgun.setTemp(temp_i);

	if (start && (iron_dev == d_t12) && pCFG->isAutoStart()) { // The T12 IRON can be started just after power-on. Default DASH mode is DM_T12_GUN
		pCore->iron.switchPower(true);
		iron_phase	= IRPH_HEATING;
		start = false;										// Prevent to start IRON automatically when return from menu
	} else {
		iron_phase	= pCore->iron.isCold()?IRPH_OFF:IRPH_COOLING;
	}
	update_screen	= 0;									// Force to redraw the screen
	tilt_time		= 0;									// No tilt change
	lowpower_time	= 0;									// Reset the low power mode time
	iron_phase_end	= 0;
	swoff_time		= 0;
	check_jbc_tm	= 0;
	edit_temp		= true;
	return_to_temp	= 0;
	gun_switch_off	= 0;
	pD->clear();
	initDevices(true, true);
	if (iron_dev == d_t12)
		pCore->iron.setCheckPeriod(6);						// Start checking the current through T12 IRON
}

MODE* MWORK::loop(void) {
	CFG*	pCFG	= &pCore->cfg;
	IRON*	pIron	= &pCore->iron;
	HOTGUN*	pHG		= &pCore->hotgun;

	manageHardwareSwitches(pCFG, pIron, pHG);

	// Check the JBC IRON tip change switch
	tDevice iron_dev = pIron->deviceType();
	if (mode_spress && iron_dev == d_jbc && pIron->isChanging()) {
		mode_spress->useDevice(d_jbc);
		return mode_spress;
	}

	if (manageEncoders()) {									// Lower encoder long pressed
		if (mode_lpress) {									// Go to the main menu
			pCore->buzz.shortBeep();
			return mode_lpress;
		}
	}
	animateFan();											// Draw the fan animated icon. The color depends on the Gun temperature

    if (HAL_GetTick() < update_screen) return this;
    update_screen = HAL_GetTick() + period;

	if (iron_phase_end > 0 && HAL_GetTick() >= iron_phase_end) {
		if (iron_dev == d_t12)
			t12PhaseEnd();
		else
			jbcPhaseEnd();
	}
	if (gun_switch_off > 0 && HAL_GetTick() >= gun_switch_off) {
		gun_switch_off = 0;
		pHG->switchPower(false);
		int16_t temp	= pCore->cfg.tempPresetHuman(d_gun);
		presetTemp(d_gun, temp);							// Restore Hot Air Gun preset temperature
		fanSpeed(false);									// Restore Hot Air Gun fan speed
		pCFG->saveConfig();									// Save configuration when the Hot Air Gun is turned-off
		devicePhase(d_gun, IRPH_OFF);
	}

    // Check the T12 IRON handle connectivity
	if (no_ambient) {										// The T12 handle was disconnected
		if (!pCore->noAmbientSensor()) {					// The T12 handle attached again
			no_ambient = false;
			pIron->setCheckPeriod(6);						// Start checking the current through the T12 IRON
			switchIron(d_t12);
		}
	} else {												// The T12 handle attached
		if (pCore->noAmbientSensor()) { 					// The T12 handle disconnected
			no_ambient = true;
			pIron->setCheckPeriod(0);						// Stop checking the current through the T12 IRON
			switchIron(d_jbc);
		}
	}

	// if T12 IRON tip is disconnected, activate Tip selection mode
	if (mode_spress && iron_dev == d_t12 && !no_ambient && !pIron->isConnected() && isACsine()) {
		if (isIronCold(iron_phase)) {						// The T12 IRON is not active
			mode_spress->useDevice(d_t12);
			return mode_spress;
		}
	}

	// Check the iron become cold
    ambient	= pCore->ambientTemp();
	if (iron_phase == IRPH_COOLING && pIron->isCold()) {
		pCore->buzz.lowBeep();
		iron_phase = IRPH_COLD;
		iron_phase_end = HAL_GetTick() + 20000;
		devicePhase(iron_dev, iron_phase);
	}

	 // Time to redraw tilt switch status
	if (iron_dev == d_t12 && iron_phase != IRPH_OFF && HAL_GetTick() > tilt_time) {
		if (t12IdleMode()) {								// tilt switch is active
			tilt_time = HAL_GetTick() + tilt_show_time;
			ironT12Used(true);								// draw the 'iron used' icon
		} else if (tilt_time > 0) {
			tilt_time = 0;
			ironT12Used(false);								// clear the 'iron used' icon
		}
	}

	// Draw low power mode
	if (iron_phase == IRPH_LOWPWR && pCore->cfg.getLowTemp(iron_dev) > 0) {
		uint32_t to = (iron_phase_end - HAL_GetTick()) / 1000;
		if (to < 100)
			pCore->dspl.timeToOff(u_upper, to);
	}

	if (iron_dev == d_jbc && iron_phase == IRPH_HEATING) {
		if (check_jbc_tm && HAL_GetTick() >= check_jbc_tm) {// Time to check the current through the JBC iron
			check_jbc_tm = 0;
			no_iron = !pIron->isConnected();
			if (no_iron) {									// The JBC iron is not detected
				pIron->switchPower(false);
				iron_phase = IRPH_COOLING;
				devicePhase(iron_dev, iron_phase);
			}
		} else {
			jbcReadyMode();									// Check the JBC IRON reaches the preset temperature
		}
	}

	adjustPresetTemp();
	drawStatus(iron_phase, ambient);
	return this;
}

// Check the hardware switches: REED switch of the Hot Air Gun and STANDBY switch of JBC iron. Returns True if the switch status changed
void MWORK::manageHardwareSwitches(CFG* pCFG, IRON *pIron, HOTGUN *pHG) {
	//  Manage the Hot Air Gun by The REED switch
	if (pHG->isReedSwitch(true)) {							// The Reed switch is open, switch the Hot Air Gun ON
		if (!pHG->isOn()) {
			uint16_t temp	= pCFG->tempPresetHuman(d_gun);
			temp 			= pCFG->humanToTemp(temp, ambient, d_gun);
			uint16_t fan	= pCFG->gunFanPreset();
			pHG->setTemp(temp);
			pHG->setFan(fan);
			pHG->switchPower(true);
			edit_temp		= true;
			return_to_temp	= 0;
			update_screen	= 0;
		}
	} else {												// The Reed switch is closed, switch the Hot Air Gun OFF
		if (pHG->isOn()) {
			uint8_t off_timeout = pCFG->getOffTimeout(d_gun);
			if (off_timeout) { 								// Put the JBC IRON to the low power mode
				uint16_t l_temp	= pCFG->getLowTemp(d_gun);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_gun);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_gun, true);
				pHG->lowPowerMode(temp);
				gun_switch_off = HAL_GetTick() + off_timeout * 60000;
				presetTemp(d_gun, l_temp);
				gunStandby();
			} else {										// Switch-off the JBC IRON immediately
				pHG->switchPower(false);
				pCFG->saveConfig();							// Save configuration when the Hot Air Gun is turned-off
				devicePhase(d_gun, IRPH_OFF);
			}
			update_screen	= 0;
		}
	}

	if (pIron->deviceType() != d_jbc) return;				// Finish function here if iron is Hakko T12

	// Manage the JBC IRON
	bool jbc_offhook = pIron->isReedSwitch(true);			// The JBC IRON is off-hook
	if (jbc_offhook) {										// The JBC IRON is off-hook, try to activate JBC IRON
		if (!no_iron) {										// JBC iron is connected
			uint16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
			if (!pIron->isOn()) {							// The JBC IRON is not powered on, setup the preset temperature
				uint16_t temp_i = pCore->cfg.humanToTemp(temp, ambient, d_jbc);
				pIron->setTemp(temp_i);
				pIron->switchPower(true);
				iron_phase = IRPH_HEATING;
				devicePhase(d_jbc, iron_phase);
				update_screen	= 0;
			} else if (iron_phase == IRPH_LOWPWR) {			// The JBC IRON was on but in low power mode
				pIron->switchPower(true);					// Return to working phase from low power mode
				presetTemp(d_jbc, temp);					// Update the preset temperature
				iron_phase = IRPH_HEATING;
				devicePhase(d_jbc, iron_phase);
				update_screen	= 0;
			}
		}
	} else {												// The JBC IRON is on-hook, try to switch it OFF or go into low power mode
		if (pIron->isOn() && isIronWorking(iron_phase)) {
			uint8_t off_timeout = pCFG->getOffTimeout(d_jbc);
			if (off_timeout) { 								// Put the JBC IRON to the low power mode
				uint16_t l_temp	= pCFG->getLowTemp(d_jbc);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_jbc, true);
				pIron->lowPowerMode(temp);
				iron_phase_end = HAL_GetTick() + off_timeout * 60000;
				iron_phase = IRPH_LOWPWR;
				devicePhase(d_jbc, iron_phase);
				presetTemp(d_jbc, l_temp);
			} else {										// Switch-off the JBC IRON immediately
				pIron->switchPower(false);
				iron_phase = IRPH_COOLING;
				devicePhase(d_jbc, iron_phase);
			}
			pCFG->saveConfig();								// Save configuration when the JBC IRON is turned-off
			update_screen	= 0;
		}
		no_iron = false;									// Re-enable JBC iron
	}
}

void MWORK::adjustPresetTemp(void) {
	tDevice dev		= pCore->iron.deviceType();
	CFG*	pCFG	= &pCore->cfg;
	IRON*	pIron	= &pCore->iron;
	uint16_t presetTemp	= pIron->presetTemp();
	uint16_t tempH     	= pCFG->tempPresetHuman(dev);
	uint16_t temp  		= pCFG->humanToTemp(tempH, ambient, dev); // Expected temperature of IRON in internal units
	if (temp != presetTemp) {								// The ambient temperature have changed, we need to adjust preset temperature
		pIron->adjust(temp);
	}
}

bool MWORK::hwTimeout(bool tilt_active) {
	CFG*	pCFG	= &pCore->cfg;

	uint32_t now_ms = HAL_GetTick();
	if (lowpower_time == 0 || tilt_active) {				// If the IRON is used, reset standby time
		lowpower_time = now_ms + pCFG->getLowTO() * 5000;	// Convert timeout (5 secs interval) to milliseconds
	}
	if (now_ms >= lowpower_time) {
		return true;
	}
	return false;
}

// Use applied power analysis to automatically power-off the IRON
void MWORK::swTimeout(uint16_t temp, uint16_t temp_set, uint16_t temp_setH, uint32_t td, uint32_t pd, uint16_t ap) {
	if (pCore->iron.deviceType() != d_t12) return;
	CFG*	pCFG	= &pCore->cfg;

	int ip = idle_pwr.read();
	if ((temp <= temp_set) && (temp_set - temp <= 4) && (td <= 200) && (pd <= 25)) {
		// Evaluate the average power in the idle state
		ip = idle_pwr.average(ap);
	}

	// Check the IRON current status: idle or used
	if (abs(ap - ip) >= 150) {								// The applied power is different than idle power. The IRON being used!
		swoff_time 		= HAL_GetTick() + pCFG->getOffTimeout(d_t12) * 60000;
		iron_phase = IRPH_NORMAL;
		devicePhase(d_t12, iron_phase);
	} else {												// The IRON is in its idle state
		if (swoff_time == 0)
			swoff_time 	= HAL_GetTick() + pCFG->getOffTimeout(d_t12) * 60000;
		uint32_t to = (swoff_time - HAL_GetTick()) / 1000;
		if (to < 100) {
			pCore->dspl.timeToOff(devPos(d_t12), to);
		} else {
			devicePhase(d_t12, IRPH_GOINGOFF);
		}
	}
}

void MWORK::t12PhaseEnd(void) {
	uint16_t t	= pCore->iron.presetTemp();
	t	= pCore->cfg.tempToHuman(t, ambient, d_t12);

	switch (iron_phase) {
		case IRPH_READY:
			iron_phase = IRPH_NORMAL;
			break;
		case IRPH_BOOST:
			pCore->iron.switchPower(true);
			iron_phase	= IRPH_HEATING;
			pCore->buzz.lowBeep();
			presetTemp(d_t12, t);							// redraw actual temperature
			break;
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			iron_phase = IRPH_COOLING;
			pCore->iron.switchPower(false);
			presetTemp(d_t12, t); 							// redraw actual temperature
			pCore->cfg.saveConfig();						// Save configuration when the T12 IRON is turned-off
			break;
		case IRPH_COLD:
			iron_phase = IRPH_OFF;
			break;
		default:
			break;
	}
	devicePhase(d_t12, iron_phase);
	iron_phase_end = 0;
}

void MWORK::jbcPhaseEnd(void) {
	uint16_t t	= pCore->iron.presetTemp();
	t	= pCore->cfg.tempToHuman(t, ambient, d_jbc);

	switch (iron_phase) {
		case IRPH_READY:
			iron_phase = IRPH_NORMAL;
			break;
		case IRPH_LOWPWR:									// The JBC IRON was on-hook for a while
			iron_phase = IRPH_COOLING;
			pCore->buzz.shortBeep();
			pCore->iron.switchPower(false);
			presetTemp(d_jbc, t);							// redraw actual temperature
			pCore->cfg.saveConfig();						// Save configuration when the JBC IRON is turned-off
			break;
		case IRPH_COLD:
			iron_phase = IRPH_OFF;
			break;
		default:
			break;
	}
	devicePhase(d_jbc, iron_phase);
	iron_phase_end = 0;
}

bool MWORK::t12IdleMode(void) {
	IRON*	pIron		= &pCore->iron;
	int temp			= pIron->averageTemp();
	int temp_set		= pIron->presetTemp();				// Now the preset temperature in internal units!!!
	uint16_t temp_set_h = pCore->cfg.tempPresetHuman(d_t12);

	uint32_t td			= pIron->tmpDispersion();			// The temperature dispersion
	uint32_t pd 		= pIron->pwrDispersion();			// The power dispersion
	int ap      		= pIron->avgPower();				// Actually applied power to the IRON

	// Check the IRON reaches the preset temperature
	if ((abs(temp_set - temp) < 6) && (td <= 500) && (ap > 0))  {
	    if (iron_phase == IRPH_HEATING) {
	    	iron_phase = IRPH_READY;
			iron_phase_end = HAL_GetTick() + 2000;
	    	pCore->buzz.shortBeep();
	    	devicePhase(d_t12, iron_phase);
	    }
	}

	bool low_power_enabled = pCore->cfg.getLowTemp(d_t12) > 0;
	bool tilt_active = false;
	if (low_power_enabled)									// If low power mode enabled, check tilt switch status
		tilt_active = pIron->isReedSwitch(pCore->cfg.isReedType());	// True if iron was used

	// If the low power mode is enabled, check the IRON status
	if (iron_phase == IRPH_NORMAL) {						// The IRON has reaches the preset temperature and 'Ready' message is already cleared
		if (low_power_enabled) {							// Use hardware tilt switch if low power mode enabled
			if (hwTimeout(tilt_active)) {					// Time to activate low power mode
				uint16_t l_temp	= pCore->cfg.getLowTemp(d_t12);
				int16_t temp	= pCore->cfg.tempPresetHuman(d_t12);
				if (l_temp >= temp)
					l_temp = temp - 10;
				temp = pCore->cfg.humanToTemp(l_temp, ambient, d_t12, true);
				pCore->iron.lowPowerMode(temp);
				iron_phase 		= IRPH_LOWPWR;				// Switch to low power mode
				devicePhase(d_t12, iron_phase);
				presetTemp(d_t12, l_temp);
				iron_phase_end	= HAL_GetTick() + pCore->cfg.getOffTimeout(d_t12) * 60000;
			}
		} else if (pCore->cfg.getOffTimeout(d_t12) > 0) {	// Do not use tilt switch, use software auto-off feature
			swTimeout(temp, temp_set, temp_set_h, td, pd, ap); // Update time_to_return value based IRON status
		}
	} else if (iron_phase == IRPH_LOWPWR && tilt_active) {	// Re-activate the IRON in normal mode
		pCore->iron.switchPower(true);
		iron_phase = IRPH_HEATING;
		uint16_t t_set = pCore->iron.presetTemp();
		t_set = pCore->cfg.tempToHuman(t_set, ambient, d_t12);
		devicePhase(d_t12, iron_phase);
		presetTemp(d_t12, t_set);							// Redraw the preset temperature
		lowpower_time = 0;									// Reset the low power mode timeout
	}
	return tilt_active;
}

void MWORK::jbcReadyMode(void) {
	IRON*	pIron		= &pCore->iron;
	int temp			= pIron->averageTemp();
	int temp_set		= pIron->presetTemp();				// Now the preset temperature in internal units!!!
	uint32_t td			= pIron->tmpDispersion();			// The temperature dispersion
	int ap      		= pIron->avgPower();				// Actually applied power to the IRON

	// Check the IRON reaches the preset temperature
	if ((abs(temp_set - temp) < 6) && (td <= 500) && (ap > 0))  {
		iron_phase = IRPH_READY;
		iron_phase_end 	= HAL_GetTick() + 2000;
		pCore->buzz.shortBeep();
		devicePhase(d_jbc, iron_phase);
	}
}

bool MWORK::manageEncoders(void) {
	HOTGUN 	*pHG		= &pCore->hotgun;
	CFG		*pCFG		= &pCore->cfg;

	uint16_t temp_set_h = pCore->u_enc.read();
    uint8_t  button		= pCore->u_enc.buttonStatus();
    tDevice  iron_dev	= pCore->iron.deviceType();
    if (button == 1) {										// The upper encoder button pressed shortly, change the working mode
    	if (iron_dev == d_t12) {
    		t12PressShort();
    		lowpower_time = 0;
    	} else {
    		jbcPressShort();
    	}
    	update_screen = 0;
    } else if (button == 2) {								// The upper encoder button was pressed for a long time
    	if (iron_dev == d_t12) {
    		t12PressLong();
    		lowpower_time = 0;
    	}
    	update_screen = 0;
    }

    if (pCore->u_enc.changed()) {							// The IRON preset temperature changed
    	if (iron_dev == d_t12) {
        	if (t12Rotate(temp_set_h)) {					// The t12 preset temperature has been changed
        		// Update the preset temperature in memory only. To save config to the flash, use saveConfig()
				pCFG->savePresetTempHuman(temp_set_h, d_t12);
				idle_pwr.reset();
				presetTemp(iron_dev, temp_set_h);
			}
    	} else {
    		if (jbcRotate(temp_set_h)) {					// The jbc preset temperature has been changed
    			// Update the preset temperature in memory only. To save config to the flash, use saveConfig()
    			pCFG->savePresetTempHuman(temp_set_h, d_jbc);
    			presetTemp(iron_dev, temp_set_h);
    		}
    	}
    	update_screen = 0;
    }

    temp_set_h		= pCore->l_enc.read();
    button			= pCore->l_enc.buttonStatus();
	if (button == 1) {										// Short press
		if (gun_switch_off > 0) {							// The Hot Air Gun is in standby mode, turn-off the Gun
			gun_switch_off = HAL_GetTick();
			return false;
		}
		// the HOT AIR GUN button was pressed, toggle temp/fan
		if (edit_temp) {									// Switch to edit fan speed
			uint16_t fan 	= pHG->presetFan();
			uint16_t min	= pHG->minFanSpeed();
			uint16_t max 	= pHG->maxFanSpeed();
			uint8_t	 step	= pHG->fanStepPcnt();
			pCore->l_enc.reset(fan, min, max, step, step<<2, false);
			edit_temp 		= false;
			temp_set_h		= fan;
			return_to_temp	= HAL_GetTick() + edit_fan_timeout;
			fanSpeed(true);
			update_screen 	= 0;
		} else {
			return_to_temp	= HAL_GetTick();				// Force to return to edit temperature
			return false;
		}
	} else if (button == 2) {								// Go to the main menu
		return true; 
	}

    if (pCore->l_enc.changed()) {							// Changed preset temperature or fan speed
		uint16_t g_temp = temp_set_h;						// In first loop the preset temperature will be setup for sure
		uint16_t t	= pHG->presetTemp();					// Internal units
		uint16_t f	= pHG->presetFan();
		t = pCFG->tempToHuman(t, ambient, d_gun);
		if (edit_temp) {
			t = temp_set_h;									// New temperature value
			presetTemp(d_gun, temp_set_h);
			uint16_t g_temp_set	= pCFG->humanToTemp(g_temp, ambient, d_gun);
			pHG->setTemp(g_temp_set);
		} else {
			f = temp_set_h;									// New fan value
			pHG->setFan(f);
			fanSpeed(true);
			return_to_temp	= HAL_GetTick() + edit_fan_timeout;
		}
		pCFG->saveGunPreset(t, f);
    }

    // The fan speed modification mode has 'return_to_temp' timeout
	if (return_to_temp && HAL_GetTick() >= return_to_temp) {// This reads the Hot Air Gun configuration Also
		uint16_t g_temp		= pCFG->tempPresetHuman(d_gun);
		uint16_t t_min		= pCFG->tempMin(d_gun);			// The minimum preset temperature
		uint16_t t_max		= pCFG->tempMax(d_gun);			// The maximum preset temperature
		uint8_t temp_step = 1;
		if (pCFG->isBigTempStep()) {						// The preset temperature step is 5 degrees
			g_temp -= g_temp % 5;							// The preset temperature should be rounded to 5
			temp_step = 5;
		}
		pCore->l_enc.reset(g_temp, t_min, t_max, temp_step, temp_step, false);
		edit_temp		= true;
		fanSpeed(false);									// Redraw in standard mode
		return_to_temp	= 0;
	}
	return false;
}

// The T12 IRON encoder button short press callback
void MWORK::t12PressShort(void) {
	switch (iron_phase) {
		case IRPH_OFF:										// The IRON is powered OFF, switch it ON
		case IRPH_COLD:
			if (no_ambient) {								// The IRON handle is not connected
				pCore->buzz.failedBeep();
				return;
			}
		case IRPH_COOLING:
		{
			uint16_t temp	= pCore->cfg.tempPresetHuman(d_t12);
			ambient 		= pCore->ambientTemp();
			temp 			= pCore->cfg.humanToTemp(temp, ambient, d_t12);
			pCore->iron.setTemp(temp);
			pCore->iron.switchPower(true);
			iron_phase	= IRPH_HEATING;
			devicePhase(d_t12, iron_phase);
		}
			break;
		default:											// Switch off the IRON
			pCore->iron.switchPower(false);
			iron_phase	= IRPH_COOLING;
			devicePhase(d_t12, iron_phase);
			pCore->cfg.saveConfig();						// Save configuration when the T12 IRON is turned-off
			presetTemp(d_t12, pCore->cfg.tempPresetHuman(d_t12));
			break;
	}
}

// The T12 IRON encoder button long press callback
void  MWORK::t12PressLong(void) {
	switch (iron_phase) {
		case IRPH_OFF:
		case IRPH_COLD:
			if (no_ambient) {								// The IRON handle is not connected
				pCore->buzz.failedBeep();
				return;
			}
		case IRPH_COOLING:
			pCore->buzz.shortBeep();
			pCore->iron.switchPower(true);
			iron_phase	= IRPH_HEATING;
			devicePhase(d_t12, iron_phase);
			break;
		case IRPH_BOOST:									// The IRON is in the boost mode, return to the normal mode
			pCore->iron.switchPower(true);
			iron_phase		= IRPH_HEATING;
			iron_phase_end	= 0;
			devicePhase(d_t12, iron_phase);
			presetTemp(d_t12, pCore->cfg.tempPresetHuman(d_t12));
			pCore->buzz.shortBeep();
			break;
		default:											// The IRON is working, go to the BOOST mode
		{
			uint8_t  bt		= pCore->cfg.boostTemp();		// Additional temperature (Degree)
			uint32_t bd		= pCore->cfg.boostDuration();	// Boost duration (s)
			if (bt > 0 && bd > 0) {
				if (!pCore->cfg.isCelsius())
					bt = (bt * 9 + 3) / 5;
				uint16_t tset		= pCore->iron.presetTemp();	// Current preset temperature, internal units
				uint16_t l_temp		= pCore->cfg.tempToHuman(tset, ambient, d_t12) + bt;
				tset				= pCore->cfg.humanToTemp(l_temp, ambient, d_t12);
				pCore->iron.boostPowerMode(tset);
				iron_phase = IRPH_BOOST;
				iron_phase_end	= HAL_GetTick() + (uint32_t)bd * 1000;
				devicePhase(d_t12, iron_phase);
				presetTemp(d_t12, l_temp);
				pCore->buzz.shortBeep();
			}
		}
			break;
	}
}

// The T12 IRON encoder rotated callback, returns true if the temperature value should be altered
// If true, the preset temperature will be updated in T12 config and on the screen
bool MWORK::t12Rotate(uint16_t new_value) {
	switch (iron_phase) {
		case IRPH_BOOST:
			return false;
		case IRPH_OFF:
		case IRPH_COLD:
		case IRPH_COOLING:
			return true;
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			pCore->iron.switchPower(true);
			iron_phase = IRPH_HEATING;
			devicePhase(d_t12, iron_phase);
			return false;
		case IRPH_HEATING:
		case IRPH_READY:
		case IRPH_NORMAL:
		default:
		{
			uint16_t temp = pCore->cfg.humanToTemp(new_value, ambient, d_t12);
			pCore->iron.setTemp(temp);
			iron_phase = IRPH_HEATING;
			return true;
		}
	}
	return false;
}

// The JBC IRON encoder button short press callback
void MWORK::jbcPressShort(void) {
	switch (iron_phase) {
		case IRPH_LOWPWR:									// Switch off the IRON
		{
			pCore->iron.switchPower(false);
			iron_phase = IRPH_COOLING;
			uint16_t temp	= pCore->cfg.tempPresetHuman(d_jbc);
			presetTemp(d_jbc, temp);
			devicePhase(d_jbc, iron_phase);
			break;
		}
		default:
			break;
	}
}

//IRPH_HEATING, IRPH_READY, IRPH_NORMAL,
//

// The JBC IRON encoder rotated callback, returns true if the temperature value should be altered
// If true, the preset temperature will be updated in JBC config and on the screen
bool MWORK::jbcRotate(uint16_t new_value) {
	switch (iron_phase) {
		case IRPH_BOOST:
		case IRPH_LOWPWR:
		case IRPH_GOINGOFF:
			return false;
		case IRPH_OFF:
		case IRPH_COLD:
		case IRPH_COOLING:
			return true;
		case IRPH_HEATING:
		case IRPH_READY:
		case IRPH_NORMAL:
		default:
		{
			uint16_t temp = pCore->cfg.humanToTemp(new_value, ambient, d_jbc);
			pCore->iron.setTemp(temp);
			iron_phase = IRPH_HEATING;
			return true;
		}
	}
	return false;
}

bool MWORK::isIronCold(tIronPhase phase) {
	return (phase == IRPH_OFF || phase == IRPH_COOLING || phase == IRPH_COLD);
}

bool MWORK::isIronWorking(tIronPhase phase) {
	return (phase == IRPH_HEATING || phase == IRPH_READY || phase == IRPH_NORMAL);
}
