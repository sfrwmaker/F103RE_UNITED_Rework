/*
 * hw.h
 *
 *
 *  2024 NOV 19
 *  	Ported from JBC controller source code, tailored to the new hardware
 *  2025 JAN 28
 *		Separate ambientTemp() into two routines to calculate stm32 temperature and steinhart sensor temperature inside Hakko T12 handle
 *		Save MCU internal temperature at startup to adjust internal temperature. As soon as the MCU temperature is higher than actual ambient temperature,
 *		return average value between MCU temperature and MCU temperature at startup.
 */

#ifndef HW_H_
#define HW_H_

#include "stat.h"
#include "iron.h"
#include "gun.h"
#include "encoder.h"
#include "display.h"
#include "config.h"
#include "buzzer.h"
#include "nls_cfg.h"

extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim8;

class HW {
	public:
		HW(void) : u_enc(&htim4), l_enc(&htim8)				{ }
		uint16_t			ambientRaw(void)				{ return t_amb.read();												}
		bool				noAmbientSensor(void)			{ return t_amb.read() >= max_ambient_value;							}
		void				updateAmbient(uint16_t value)	{ t_amb.update(value);												}
		void				updateIntTemp(uint16_t vref, uint16_t value)
															{ vrefint.update(vref); t_stm32.update(value);						}
		void				initAmbient(uint16_t value)		{ t_amb.reset(value);												}
		void				updateTiltSwitch(bool on)		{ if (d_t12 == iron.deviceType()) iron.updateReedStatus(on);		}
		void				updateJBCswitch(bool offhook) 	{ if (d_jbc == iron.deviceType()) iron.updateReedStatus(offhook);	}
		CFG_STATUS			init(uint16_t iron_temp, uint16_t gun_temp, uint16_t ambient, uint16_t vref, uint32_t t_mcu);
		int32_t				ambientTemp(void);				// T12 IRON ambient temperature
		CFG			cfg;
		NLS			nls;
		DSPL		dspl;
		IRON		iron;									// T12 or JBC IRON instance, depends on jbc_sw status
		RENC		u_enc, l_enc;							// Upper encoder and lower encoder
		HOTGUN		hotgun;
		BUZZER		buzz;
	private:
		int32_t				internalTemp(int32_t raw_stm32);
		int32_t 			steinhartTemp(int32_t raw_ambient);
		EMP_AVERAGE 	t_amb;								// Exponential average of the ambient temperature
		EMP_AVERAGE		vrefint;							// Exponential average of the vrefint
		EMP_AVERAGE		t_stm32;							// Exponential average of the internal MCU temperature
		int8_t			start_temp			= 0; 			// Startup temperature
		const uint8_t	ambient_emp_coeff	= 30;			// Exponential average coefficient for ambient temperature
		const uint16_t	max_ambient_value	= 3900;			// About -30 degrees. If the soldering IRON disconnected completely, "ambient" value is greater than this
		const uint8_t 	sw_jbc_len			= 15;			// JBC IRON switch history length
		const uint8_t	sw_off_value		= 14;			// JBC IRON switch off threshold
		const uint8_t	sw_on_value			= 20;			// JBC IRON switch on threshold
};

#endif
