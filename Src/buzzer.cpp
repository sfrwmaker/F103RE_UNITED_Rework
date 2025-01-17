/*
 * buzzer.cpp
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 *
 */

#include "buzzer.h"
#include "main.h"

void BUZZER::playTone(uint16_t period_mks, uint16_t duration_ms) {
	BUZZER_TIM.Instance->ARR 	= period_mks-1;
	BUZZER_TIM.Instance->CCR1 	= period_mks >> 1;
	HAL_Delay(duration_ms);
	BUZZER_TIM.Instance->CCR1 	= 0;
}

void BUZZER::shortBeep(void) {
	if (!enabled) return;
//	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
//	playTone(284, 160);
//	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
	playSong(short_beep);
}

void BUZZER::doubleBeep(void) {
	if (!enabled) return;
//	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
//	playTone(284, 160);
//	HAL_Delay(100);
//	playTone(284, 160);
//	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
	playSong(double_beep);
}

void BUZZER::lowBeep(void) {
	if (!enabled) return;
//	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
//	playTone(2840, 160);
//	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
	playSong(low_beep);
}

void BUZZER::failedBeep(void) {
	if (!enabled) return;
//	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
//	playTone(284, 160);
//	HAL_Delay(50);
//	playTone(2840, 60);
//	HAL_Delay(50);
//	playTone(1420, 160);
//	HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
	playSong(failed_beep);
}

void BUZZER::playSong(const uint16_t *song) {
	if (next_note) return;									// The song is already playing
	next_note = song;
	HAL_TIM_PWM_Start(&BUZZER_TIM,  TIM_CHANNEL_1);
	HAL_TIM_Base_Start_IT(&PERIOD_TIM);
}

void BUZZER::playSongCB(void) {
	uint16_t period   = *next_note++;
	uint16_t duration = *next_note++;
	if (duration == 0) {									// Stop playing
		HAL_TIM_PWM_Stop(&BUZZER_TIM,  TIM_CHANNEL_1);
		HAL_TIM_Base_Stop_IT(&PERIOD_TIM);
		BUZZER_TIM.Instance->CCR1 = 0;
		next_note	= 0;
		return;
	}
	BUZZER_TIM.Instance->ARR 	= period-1;
	BUZZER_TIM.Instance->CCR1 	= period >> 1;
	PERIOD_TIM.Instance->ARR	= duration-1;
}
