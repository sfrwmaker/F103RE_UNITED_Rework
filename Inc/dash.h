/*
 * dash.h
 *  Author: Alex
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 */

#ifndef _DASH_H_
#define _DASH_H_
#include "mode.h"

typedef enum { DM_T12_GUN = 0, DM_JBC_GUN} tDashMode;

typedef enum { IRPH_OFF = 0, IRPH_HEATING, IRPH_READY, IRPH_NORMAL, IRPH_BOOST, IRPH_LOWPWR, IRPH_GOINGOFF,
				IRPH_COOLING, IRPH_COLD } tIronPhase;

// Main working mode dashboard class
class DASH : public MODE {
	public:
		DASH(HW *pCore)	: MODE(pCore)						{ }
		void			init();
		bool			setMode(tDashMode dm);
		void			drawStatus(tIronPhase iron_phase, int16_t ambient);
		void 			animateFan(void);
		void			ironT12Used(bool active);
		bool 			switchIron(tDevice dev);
		tUnitPos		devPos(tDevice dev);
		void 			devicePhase(tDevice dev, tIronPhase phase);
		void			presetTemp(tDevice dev, uint16_t temp);
		void			fanSpeed(bool modify);
		void			gunStandby(void);
	protected:
		void 			initEncoders(tDevice i_dev, int16_t i_value, uint16_t g_value);
		void 			changeIronShort(void);
		void  			changeIronLong(void);
		bool 			t12Encoder(uint16_t new_value);
		bool			initDevices(bool init_iron, bool init_gun);
		bool			no_ambient		= false;			// The ambient sensor (in hakko T12 handle) detected
		bool			no_iron			= false;			// The current through the iron not detected flag
		uint32_t		fan_animate		= 0;				// Time when draw new fan animation
		bool			fan_blowing		= false;			// Used to draw grey or animated and colored fan
		tIronPhase		iron_phase		= IRPH_OFF;			// Current T12 IRON phase
		int16_t			ambient			= 100;				// The displayed ambient temperature (impossible init value)
};



#endif
