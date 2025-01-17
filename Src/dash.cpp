/*
 * dash.cpp
 *   Author: Alex
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 */

#include "dash.h"
#include "display.h"
#include "unit.h"

void DASH::init(void) {
	fan_animate		= 0;
	fan_blowing		= !pCore->hotgun.isCold();
	no_ambient		= pCore->noAmbientSensor();				// Hakko T12 iron handle has an ambient sensor
	tDevice iron_dev = d_t12;
	if (no_ambient) {										// Determine the iron type by ambient sensor
		iron_dev		= d_jbc;
		no_iron			= false;							// Assume jbc is iron connected, do not check the current through the iron
	} else {
		no_iron			= !pCore->iron.isConnected();
	}
	if (pCore->iron.deviceType() != iron_dev)
		pCore->iron.changeType(iron_dev);
	ambient = 100;											// Impossible value to force redraw
}

void DASH::drawStatus(tIronPhase iron_phase, int16_t ambient) {
	DSPL*	pD		= &pCore->dspl;
	CFG*	pCFG	= &pCore->cfg;
	IRON*	pIron	= &pCore->iron;
	HOTGUN*	pHG		= &pCore->hotgun;

	// Get parameters of the iron
	tDevice iron_dev 	= pIron->deviceType();
	uint16_t temp  		= pIron->averageTemp();
	uint16_t i_temp_h 	= pCFG->tempToHuman(temp, ambient, iron_dev);
	temp				= pIron->presetTemp();
	uint16_t i_temp_s	= pCFG->tempToHuman(temp, ambient, iron_dev);
	if (iron_phase == IRPH_LOWPWR) {
		i_temp_s		= pCFG->getLowTemp(iron_dev);
	}
	uint8_t	 i_pwr		= pIron->avgPowerPcnt();

	// Get Parameters of the hot gun
	temp				= pHG->averageTemp();
	uint16_t g_temp_h	= pCFG->tempToHuman(temp, ambient, d_gun);
	temp				= pHG->presetTemp();
	uint16_t g_temp_s	= pCFG->tempToHuman(temp, ambient, d_gun);
	uint8_t	 g_pwr		= pHG->avgPowerPcnt();

	bool celsius = pCFG->isCelsius();

	// Draw status of iron
	bool iron_on = (iron_phase != IRPH_OFF && iron_phase != IRPH_COLD);
	pD->drawTempGauge(i_temp_h-i_temp_s, u_upper, iron_on);
	pD->drawPower(i_pwr, u_upper);
	// Show the IRON temperature 
	if (iron_phase == IRPH_HEATING) {
		pD->animatePower(u_upper, i_temp_h - i_temp_s);		// Draw colored power icon
	} else if (no_iron || iron_phase == IRPH_OFF) {			// Prevent the temperature changing when the IRON is OFF
		i_temp_h = ambient;
	}
	if (iron_phase == IRPH_COOLING) {
		pD->animateTempCooling(i_temp_h, celsius, u_upper);
	} else {
		pD->drawTemp(i_temp_h, u_upper);
	}
	// Correct the preset temperature depending on current IRON mode
	if (iron_phase == IRPH_LOWPWR) {
		uint16_t l_temp	= pCore->cfg.getLowTemp(iron_dev);
		if (l_temp > 0)
			i_temp_s = l_temp;
	} else if (iron_dev == d_t12 && iron_phase == IRPH_BOOST) {
		uint8_t  bt		= pCFG->boostTemp();				// Additional temperature (Degree)
		if (!pCFG->isCelsius())
			bt = (bt * 9 + 3) / 5;
		uint16_t tset = pIron->presetTemp();				// Current preset temperature, internal units
		i_temp_s = pCFG->tempToHuman(tset, ambient, d_t12) + bt;
	}

	// Draw the status of the HOT IRON GUN
	if (!pHG->isFanWorking()) {
		if (fan_blowing) {
			pD->stopFan();
			fan_blowing = false;
			pD->msgOFF(u_lower);
		}
		g_temp_h = ambient;
	} else {
		fan_blowing	= true;
		if (pHG->isOn()) {
			pD->animatePower(u_lower, g_temp_h - g_temp_s);
		}
	}

	if (fan_blowing && !pHG->isOn()) {
		pD->animateTempCooling(g_temp_h, celsius, u_lower);
	} else {
		pD->drawTemp(g_temp_h, u_lower);
	}
	pD->drawTempGauge(g_temp_h-g_temp_s, u_lower, fan_blowing);
	pD->drawPower(g_pwr, u_lower);
	if (this->ambient != ambient) {
		this->ambient = ambient;
		pD->drawAmbient(ambient, pCFG->isCelsius());
	}
}

void DASH::animateFan(void) {
	HOTGUN *pHG = &pCore->hotgun;
	if (pHG->isFanWorking() && HAL_GetTick() >= fan_animate && pHG->isConnected()) {
		int16_t  temp		= pHG->averageTemp();
		int16_t  temp_s		= pHG->presetTemp();
		pCore->dspl.animateFan(temp-temp_s);
		fan_animate = HAL_GetTick() + 100;
	}
}

void DASH::ironT12Used(bool active) {
	if (pCore->iron.deviceType() == d_t12)
		pCore->dspl.ironActive(active, u_upper);
}

bool DASH::switchIron(tDevice dev) {
	if (dev == pCore->iron.deviceType())					// No iron device type change
		return false;
	return setMode((dev == d_jbc)?DM_JBC_GUN:DM_T12_GUN);
}

bool DASH::setMode(tDashMode dm) {
	tDevice new_iron_dev 	= d_t12;
	tDevice iron_dev 		= pCore->iron.deviceType();
	if (iron_dev == d_t12)
		no_iron = no_ambient && !pCore->iron.isConnected();

	switch (dm) {
		case DM_JBC_GUN:
			new_iron_dev = d_jbc;
			break;
		case DM_T12_GUN:
		default:
			break;
	}
	bool init_iron = (new_iron_dev != iron_dev);
	if (init_iron)
		pCore->iron.changeType(new_iron_dev);
	return initDevices(init_iron, false);
}

bool DASH::initDevices(bool init_iron, bool init_gun) {
	DSPL 	*pD 		= &pCore->dspl;
	CFG		*pCFG 		= &pCore->cfg;
	tDevice iron_dev	= pCore->iron.deviceType();
	bool mode_changed = false;
	uint16_t i_preset = init_iron?pCFG->tempPresetHuman(iron_dev):0;
	uint16_t g_preset = pCFG->tempPresetHuman(d_gun);
	initEncoders(iron_dev, i_preset, g_preset);
	if (init_iron) {
		mode_changed = true;
		pD->drawTipName(pCFG->tipName(iron_dev), pCFG->isTipCalibrated(iron_dev), u_upper);
		pD->drawTempSet(i_preset, u_upper);
		pD->ironActive(false, u_upper);
		// Draw new device power status icon
		if (iron_phase == IRPH_OFF || iron_phase == IRPH_COOLING) {
			pD->msgOFF(u_upper);
		} else if (iron_phase == IRPH_LOWPWR) {
			pD->msgStandby(u_upper);
		} else if (iron_phase == IRPH_NORMAL) {
			pD->msgNormal(u_upper);
		}
	}
	if (init_gun) {
		mode_changed = true;
		int16_t ambient	= pCore->ambientTemp();
		bool celsius 	= pCore->cfg.isCelsius();
		pD->drawFanPcnt(pCore->hotgun.presetFanPcnt());
		pD->drawAmbient(ambient, celsius);
		pD->stopFan();
		if (pCore->hotgun.isOn())
			pD->msgON(u_lower);
		else
			pD->msgOFF(u_lower);
		pD->drawTempSet(g_preset, u_lower);
		fan_blowing		= false;
		fan_animate		= 0;
	}
	return mode_changed;
}

void DASH::initEncoders(tDevice i_dev, int16_t i_value, uint16_t g_value) {
	CFG*	pCFG	= &pCore->cfg;

	uint16_t it_min		= pCFG->tempMin(d_t12);				// The minimum IRON preset temperature, defined in vars.h
	uint16_t it_max		= pCFG->tempMax(d_t12);				// The maximum IRON preset temperature
	uint16_t gt_min		= pCFG->tempMin(d_gun);				// The minimum GUN preset temperature, defined in vars.h
	uint16_t gt_max		= pCFG->tempMax(d_gun);				// The maximum GUN preset temperature
	uint8_t temp_step = 1;
	if (pCFG->isBigTempStep()) {							// The preset temperature step is 5 degrees
		temp_step = 5;
		i_value -= i_value % 5;								// The preset temperature should be rounded to 5
		g_value -= g_value % 5;
	}

	if (i_value)											// The upper device can be T12 or JBC. It cannot manage the Hot Air Gun
		pCore->u_enc.reset(i_value, it_min, it_max, temp_step, temp_step, false);
	if (g_value)
		pCore->l_enc.reset(g_value, gt_min, gt_max, temp_step, temp_step, false);
}

tUnitPos DASH::devPos(tDevice dev) {
	if (dev == pCore->iron.deviceType())
		return u_upper;
	if (dev == d_gun)
		return u_lower;
	return u_none;
}

void DASH::devicePhase(tDevice dev, tIronPhase phase) {
	tUnitPos pos = devPos(dev);
	if (pos == u_none)
		return;
	DSPL 	*pD 	= &pCore->dspl;

	switch (phase) {
		case IRPH_HEATING:
			pD->msgON(pos);
			break;
		case IRPH_READY:
			pD->msgReady(pos);
			break;
		case IRPH_NORMAL:
			pD->msgNormal(pos);
			break;
		case IRPH_BOOST:
			pD->msgBoost(pos);
		case IRPH_LOWPWR:
			pD->msgStandby(pos);
			break;
		case IRPH_GOINGOFF:
			pD->msgIdle(pos);
			break;
		case IRPH_COLD:
			pD->msgCold(pos);
			break;
		case IRPH_OFF:
		case IRPH_COOLING:
		default:
			pD->msgOFF(pos);
			break;
	}
}

void DASH::presetTemp(tDevice dev, uint16_t temp) {
	tUnitPos pos = devPos(dev);
	if (pos == u_none)
		return;
	pCore->dspl.drawTempSet(temp, pos);
}

void DASH::fanSpeed(bool modify) {
		pCore->dspl.drawFanPcnt(pCore->hotgun.presetFanPcnt(), modify);
}

void DASH::gunStandby(void) {
		pCore->dspl.drawGunStandby();
}
