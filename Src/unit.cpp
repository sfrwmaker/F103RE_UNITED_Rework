/*
 * unit.cpp
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 */

#include "unit.h"

void UNIT::init(uint8_t c_len, uint16_t c_min, uint16_t c_max, uint8_t s_len, uint16_t s_min, uint16_t s_max) {
	current.init(c_len,	c_min,	c_max);
	sw.init(s_len,		s_min, 	s_max);
	change.init(s_len,	s_min, s_max);
	sw.reset(0);											// Make sure the JBC iron is on-hook at startup
	change.reset(0);										// The JBC change button is not pressed
}

bool UNIT::isReedSwitch(bool reed) {
	if (reed)
		return sw.status();									// TRUE if switch is open (IRON in use)
	return sw.changed();									// TRUE if tilt status has been changed
}
