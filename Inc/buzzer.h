/*
 * buzzer.h
 *
 *  2024 DEC 01
 *  	Ported from JBC controller source code, tailored to the new hardware
 *  	Modified the class to play songs in non-blocking mode using PERIOD_TIM timer to check note duration
 *
 */

#ifndef BUZZER_H_
#define BUZZER_H_

#ifndef __BUZZ_H
#define __BUZZ_H
#include "main.h"

/*
 * BUZZER_TIM should has tick time 1 mks
 * PERIOD_TIP should has tick time 100 mks
 */
#define BUZZER_TIM			htim1
extern 	TIM_HandleTypeDef 	BUZZER_TIM;
#define PERIOD_TIM			htim6
extern  TIM_HandleTypeDef 	PERIOD_TIM;

class BUZZER {
	public:
		BUZZER(void)					{ }
		void		activate(bool e)	{ enabled = e; BUZZER_TIM.Instance->CCR1 = 0; }
		void		lowBeep(void);
		void		shortBeep(void);
		void		doubleBeep(void);
		void		failedBeep(void);
		void		playSongCB(void);
	private:
		void		playTone(uint16_t period_mks, uint16_t duration_ms);
		void		playSong(const uint16_t *song);
		bool		enabled = true;
		const uint16_t *next_note		= 0;
		const uint16_t short_beep[4] 	= { 284,  1600, 0, 0 };
		const uint16_t double_beep[8] 	= { 284,  1600, 0, 1000, 284,  1600, 0, 0 };
		const uint16_t low_beep[4]		= { 2840, 1600, 0, 0 };
		const uint16_t failed_beep[12]	= { 284,  1600, 0, 500 , 2840, 600,  0, 500, 1420, 1600, 0, 0 };
};

#endif

#endif		/* BUZZER_H_ */
