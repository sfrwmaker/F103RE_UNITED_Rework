/*
 * core.cpp
 *
 *  2024 NOV 16, v1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
 * 	2025 MAR 06, v1.01
 * 		Implemented flash erase procedure, see FERASE class initialization
 * 		Changed class initialization because the MFAIL pointer now is inside the HW class
 * 	2025 JUN 03, v1.02
 * 		Changed the algorithm how the Hot Air Gun managed, see detailed description below
 * 		Changed the TIM3 initialization
 * 		Created calculateGunPowerData() routine
 * 	2025 OCT 07, v1.03
 * 		Changed the size of adc1_buff array to 8, see #define ADC1_CUR statement
 * 		Changed the HAL_ADC_ConvCpltCallback() routine to calculate Hot Gun temperature more accurately by 4 samples
 * 	2025 NOV 11, v1.03
 * 		Replaced the MCU temperature readings with preset value via preference menu. Used as ambient temperature when T12 handle is not connected.
 *
 *  Hardware configuration:
 *  Analog pins:
 *  A2	- IRON temperature, ADC3
 *  A4	- IRON current, ADC1
 *  A5	- FAN current, ADC1
 *  A6	- GUN temperature, ADC1
 *  C0  - Ambient temperature, ADC3
 *  TIM2:
 *  A0	- TIM2_CH1, IRON power [0-1999]
 *  A1	- TIM2_CH2,	FAN  power [0-1999]
 *  	  TIM2_CH3, Output compare (1) to check current
 *  	  TIM2_CH4, Output compare (1986) to check temperature
 *  TIM3:
 *  D2	- TIM3_ETR, AC zero signal read - Timer reset signal
 *  B1	- TIM3_CH4, Hot Air Gun power [0-max_gun_pwr]
 *  TIM4:
 *  B6	- TIM4_CH1, I_ENC_L
 *  B7	- TIM4_CH2, I_ENC_R
 *  TIM8:
 *  C6	- TIM8_CH1, G_ENC_L
 *  C7	- TIM8_CH2, G_ENC_R
 *  TIM1:
 *  A8	- TIM1_CH1, Buzzer
 *  TIM5:
 *  PA3	- TIM5_CH4, TFT brightness
 *
 *
 *  How the Hot Air Gun powered.
 *  The AC power outlet frequency is 50 Hz in Europe and 60 Hz in USA. As soon as AC signal after full diode rectifier has a positive half-period shapes its frequency is 100 Hz or 120 Hz respectively.
 *  This signal is coming from the power board as a AC_ZERO interrupts. The TIM3 timer is clocked by the internal clock and has period of 25.600 mS (the timer counter ticks from 0 to 255).
 *  The period between two AC_ZERO signals is 10 mS (8.33 ms in USA). The AC_ZERO signal resets the TIM3 counter, so the TIM3 counter never reached its period for sure.
 *  To avoid the AC source distortion, the TRIAC in the power board are managed to propagate whole halh-period shape or to completely block it.
 *  To manage the power of the Hot Air Gun, the controller uses a means of 120 half-period shapes (1.2 secs in Europe or 1 sec is USA) because the number 120 has a multiple dividers: 2,3,4,5,6, etc.
 *  So it easy to distribute many active half-period shapes from 0 to 120 in 120-element array evenly. 120 is a period of checking the Hot Air Gun temperature and calculate the required power to be applied.
 *  As soon as required power calculated (0-120 half-period shapes) this number of half-period shapes is distributed into 120 elements DMA buffer (gun_pwr) by the calculateGunPowerData() routine.
 *  This DMA buffer then sent to the TIM3->CH4 to manage the PWM signal to activate the TRIAC on the Power board. Active pulse encoded as 70 (TIM3 ticks) and inactive pulse is encoded as 0.
 *  As mentioned before, the TIM3 timer counts from 0 to 100 (or 83 in USA) before AC_ZERO interrupt reset the timer and the 70-ticks long active pulse activates the TRIAC at the AV wave beginning
 *  and goes down before the sine pulse ends, but the TRIAC keeps open until the AC sine wave goes through the zero, so the half-period shape will propagate to the Hot Air Gun completely.
 *  From the other side, the TIM3 PWM channel goes down at 70-th timer tick and power-off the Hot Air Gun at the beginning of the next half-period shape for sure.
 */

#include <math.h>
#include "core.h"
#include "hw.h"
#include "mode.h"
#include "work_mode.h"
#include "menu.h"
#include "vars.h"

// Activated ADC Ranks Number (hadc1.Init.NbrOfConversion)
#define ADC1_CUR 			(6)
#define ADC3_TEMP			(5)
#define MAX_GUN_POWER		(120)

extern ADC_HandleTypeDef	hadc1;
extern ADC_HandleTypeDef	hadc3;
extern TIM_HandleTypeDef	htim2;
extern TIM_HandleTypeDef	htim3;

extern uint8_t				usb_flash_busy;					// Defined in usbd_storage_if.c

volatile static uint32_t	errors		= 0;

typedef enum { ADC_IDLE, ADC_CURRENT, ADC_TEMP } t_ADC_mode;
volatile static t_ADC_mode	adc_mode = ADC_IDLE;
volatile static uint16_t	adc1_buff[ADC1_CUR];			// Current data: IRON, FAN, (GUN temperature)*4
volatile static uint16_t	adc3_buff[ADC3_TEMP];			// Temperature data: IRON * 4, AMBIENT
volatile static uint32_t	gtim_last_ms	= 0;			// Time when the last AC_ZERO interrupt received
volatile static	bool		ac_sine		= false;			// Flag indicating that TIM3 has received the external interrupt on AC_ZERO pin
volatile static uint8_t		gun_pwr[MAX_GUN_POWER*2] = {0};	// The HOT GUN power PWM buffer
volatile static bool		adc_manual	= false;			// Flag indicating that we have to manage ADC DMA buffer manually
static 	EXPA				gtim_period;					// gun timer period (ms)
static  uint16_t  			max_iron_pwm	= 0;			// Max value should be less than TIM3.CH3 value by 40. Will be initialized later
const static	uint32_t	check_sw_period = 100;			// IRON switches check period, ms

static HW		core;										// Hardware core (including all device instances)

// MODE instances
static	MFAIL			fail(&core);
static	MWORK			work(&core);
static	MSLCT			iselect(&core);
static	MTACT			activate(&core, &fail);
static	MCALIB			calib_auto(&core);
static	MCALIB_MANUAL	calib_manual(&core);
static	MCALMENU		calib_menu(&core, &calib_auto, &calib_manual);
static	MTPID			manual_pid(&core);
static 	MAUTOPID		auto_pid(&core);
static	MENU_PID		pid_menu(&core, &manual_pid, &auto_pid);
static  MDEBUG			debug(&core);
static	FFORMAT			format(&core, &fail);
static	MSETUP			param_menu(&core, &pid_menu);
static	MENU_T12		t12_menu(&core, &calib_menu);
static	MENU_JBC		jbc_menu(&core, &calib_menu);
static  MENU_GUN		gun_menu(&core, &calib_manual);
static  FERASE			flash_erase(&core, &fail);
static  MABOUT			about(&core, &flash_erase);
static	MMENU			main_menu(&core, &iselect, &param_menu, &activate, &t12_menu, &jbc_menu, &gun_menu, &about);
static	MODE*           pMode = &work;

bool 		isACsine(void)		{ return ac_sine; 				}
uint16_t	gtimPeriod(void)	{ return gtim_period.read();	}

// Synchronize TIM2 timer to AC power. The main timer managing IRON and FAN
static uint16_t syncAC(uint16_t tim_cnt) {
	uint32_t to = HAL_GetTick() + 300;						// The timeout
	while (HAL_GetTick() < to) {							// Prevent hang
		if (TIM3->CNT == 0) {								// AC_ZERO interrupt restarts the TIM# timer
			TIM2->CNT = tim_cnt;							// Synchronize TIM2 to AC power zero crossing signal
			break;
		}
	}
	// Checking the TIM2 has been synchronized
	to = HAL_GetTick() + 300;
	while (HAL_GetTick() < to) {
		if (TIM3->CNT == 0) {
			return TIM2->CNT;
		}
	}
	return TIM2->ARR+1;										// This value is bigger than TIM2 period, the TIM2 has not been synchronized
}

// Calculates the PWM value data for TIMER to supply power to the heater
// Each AC-outlet peak (100 Hz in Russia and 60 Hz in US) resets the timer and make the timer to supply power
// The PWM values can be in two states: supply power for the half-period (peak) or not
static void calculateGunPowerData(volatile uint8_t *data, uint8_t max_power, uint8_t pwr) {
	const uint8_t active_pulse = 70;
	uint8_t on	= active_pulse;
	uint8_t off = 0;
	if (pwr > (max_power >> 1)) {							// In case the pwr is greater than half of maximum power, calculate positions of "empty" peaks
		if (pwr > max_power) pwr = max_power;
		on	= 0;
		off = active_pulse;
		pwr = max_power - pwr;
	}
	if (pwr == 0) {											// No power supplied at all, empty all PWM slots
		for (uint8_t i = 0; i < max_power; ++i)
			data[i] = off;
		return;
	}
	uint8_t slots	= max_power / pwr;						// Number of PWM slots per each "powered" peak (0 .. max_power/2)
	uint8_t remain	= max_power % pwr;						// The division remainder
	uint8_t pos		= slots >> 1;							// Put the "powered" peak in to the center of the slot
	int8_t extra	= 0;									// Extra position remainder (extra/pwr)
	for (uint8_t i = 0; i < max_power; ++i) {
		if (i < pos) {
			data[i] = off;
		} else {
			data[i] = on;
			pos += slots;
			extra += remain;
			if (extra + (remain>>1) >= pwr) {
				++pos;
				extra -= pwr;
			}
		}
	}
}

static void powerOffGun(void) {
	for (uint16_t i = 0; i < MAX_GUN_POWER * 2; ++i) {
		gun_pwr[i] = 0;
	}
}

extern "C" void setup(void) {
	HAL_ADCEx_Calibration_Start(&hadc1);					// Calibrate both ADCs
	HAL_ADCEx_Calibration_Start(&hadc3);

	adc_manual = true;										// Manage DMA reading buffer manually, see HAL_ADC_ConvCpltCallback()
	HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_buff, ADC3_TEMP);	// ADC3 reads the IRON temperature and ambient temperature
	while (adc_manual) { }									// Wait for ADC readings
	uint16_t iron_temp = adc3_buff[0];
	for (uint8_t i = 1; i < 4; ++i)							// adc3_buff[0-3] is the IRON temperature
		iron_temp += adc3_buff[i];
	iron_temp += 2; iron_temp >>= 2;
	uint16_t ambient = adc3_buff[4];						// adc3_buff[4] is ambient temperature (sensor inside T12 handle)

	// ADC1 reads [iron_current, fan_current, gun_temp]
	adc_manual = true;										// Manage DMA reading buffer manually, see HAL_ADC_ConvCpltCallback()
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buff, ADC1_CUR);
	while (adc_manual) { }									// Wait for ADC readings
	uint16_t gun_temp	= 0;
	for (uint8_t i = 2; i < 6; ++i) {
		gun_temp += adc1_buff[i];							// The Gun temperature is measured 4 times
	}
	gun_temp >>= 2;											// Divide by 4

	gtim_period.length(10);
	gtim_period.reset(1000);								// Default TIM1 period, ms
	max_iron_pwm	= htim2.Instance->CCR4 - 40;			// Stop supplying power in 40 mkS before start checking IRON temperature

	CFG_STATUS cfg_init = core.init(iron_temp, gun_temp, ambient);

	HAL_TIM_Base_Start_IT(&htim3);
	HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (const uint32_t*)gun_pwr, MAX_GUN_POWER*2); // PWM signal of Hot Air Gun
	HAL_TIM_PWM_Start(&htim2, 	TIM_CHANNEL_1);				// PWM signal of the IRON
	HAL_TIM_PWM_Start(&htim2, 	TIM_CHANNEL_2);				// PWM signal of the FAN
	HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_3);				// Check the current through the IRON and FAN
	HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_4);				// Calculate power of the IRON

	// Setup main mode parameters: return mode, short press mode, long press mode
	work.setup(&main_menu, &iselect, &main_menu);
	iselect.setup(&work, &activate, &main_menu);
	activate.setup(&work, &work, &main_menu);
	calib_auto.setup(&work, &work, &work);
	calib_manual.setup(&calib_menu, &work, &work);
	calib_menu.setup(&work, &work, &work);
	fail.setup(&work, &work, &work);
	manual_pid.setup(&work, &work, &work);
	auto_pid.setup(&work, &manual_pid, &manual_pid);
	pid_menu.setup(&main_menu, &work, &work);
	param_menu.setup(&main_menu, &work, &work);
	t12_menu.setup(&main_menu, &work, &work);
	jbc_menu.setup(&main_menu, &work, &work);
	gun_menu.setup(&main_menu, &work, &work);
	main_menu.setup(&work, &work, &work);
	about.setup(&work, &work, &debug);
	debug.setup(&work, &work, &work);
	auto_pid.setup(&work, &manual_pid, &manual_pid);
	format.setup(&work, 0, 0);
	flash_erase.setup(&work, 0, 0);

	core.dspl.clear();
	if (usb_flash_busy) {									// Attached via usb port to the computer to upload files
		fail.setup(&fail, &fail, &fail);					// Do not exit the fail mode
		fail.setMessage(MSG_UPDATE_FLASH);
		pMode	= &fail;
	} else {
		switch (cfg_init) {
			case CFG_NO_TIP:
				pMode	= &activate;						// No tip configured, run tip activation menu
				break;
			case CFG_READ_ERROR:							// Failed to read FLASH
				fail.setMessage(MSG_FLASH_READ_ERR);
				fail.setup(&fail, &fail, &format);			// Do not enter the main working mode
				pMode	= &fail;
				break;
			case CFG_NO_FILESYSTEM:
				pMode	= &format;
				break;
			case CFG_NO_TIP_LIST:
				fail.setMessage(MSG_NO_TIP_LIST);
				pMode	= &fail;
				break;
			default:
				break;
		}
	}
	syncAC(0);												// Synchronize TIM2 timer to AC power. Parameter is TIM2 counter value when AC_ZERO interrupt occurred
	uint8_t br = core.cfg.getDsplBrightness();
	core.dspl.BRGT::set(br);
	// Turn-on the display backlight immediately in the debug mode
#ifdef DEBUG_ON
	core.dspl.BRGT::on();
#endif
	HAL_Delay(200);
	pMode->init();
}

extern "C" void loop(void) {
	static uint32_t	check_sw		= 0;					// Time when check iron switches status (ms)

	if (HAL_GetTick() > check_sw) {
		check_sw = HAL_GetTick() + check_sw_period;
		GPIO_PinState pin = HAL_GPIO_ReadPin(TILT_SW_GPIO_Port, TILT_SW_Pin);
		core.updateTiltSwitch(GPIO_PIN_SET == pin);			// Update T12 TILT switch status
		pin = HAL_GPIO_ReadPin(JBC_STBY_GPIO_Port, JBC_STBY_Pin);
		core.updateJBCswitch(GPIO_PIN_SET == pin);			// Switch active when the JBC handle is off-hook
		pin = HAL_GPIO_ReadPin(JBC_CHANGE_GPIO_Port, JBC_CHANGE_Pin);
		core.iron.updateChangeStatus(GPIO_PIN_RESET == pin); // Switch active when the JBC tip on change connector
		pin = HAL_GPIO_ReadPin(REED_SW_GPIO_Port, REED_SW_Pin);
		core.hotgun.updateReedStatus(GPIO_PIN_SET == pin);	// Switch active when the Hot Air Gun handle is off-hook
	}

	MODE* new_mode = pMode->returnToMain();
	if (new_mode && new_mode != pMode) {
		core.buzz.doubleBeep();
		core.iron.switchPower(false);
		TIM2->CCR1	= 0;									// Switch-off the IRON power immediately
		powerOffGun();
		pMode->clean();
		pMode = new_mode;
		pMode->init();
		return;
	}
	new_mode = pMode->loop();
	if (new_mode != pMode) {
		if (new_mode == 0) new_mode = &fail;				// Mode Failed
		core.iron.switchPower(false);
		core.hotgun.switchPower(false);
		core.iron.setCheckPeriod(0);						// Stop checking IRON
		TIM2->CCR1	= 0;									// Switch-off the IRON power immediately
		powerOffGun();
		pMode->clean();
		pMode = new_mode;
		pMode->init();
	}

	// Adjust display brightness
	if (core.dspl.BRGT::adjust()) {
		HAL_Delay(5);
	}
}

static bool adcStartCurrent(void) {							// Check the current by ADC3
    if (adc_mode != ADC_IDLE) {								// Not ready to check analog data; Something is wrong!!!
    	TIM2->CCR1 = 0;										// Switch off the IRON
    	TIM2->CCR2 = 0;										// Switch off the FAN
    	TIM3->CCR4 = 0;										// Switch off the Hot Air Gun
    	++errors;
		return false;
    }
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buff, ADC1_CUR);
	adc_mode = ADC_CURRENT;
	return true;
}

static bool adcStartTemp(void) {							// Check the temperature by ADC1 & ADC2
    if (adc_mode != ADC_IDLE) {								// Not ready to check analog data; Something is wrong!!!
    	TIM2->CCR1 = 0;										// Switch off the IRON
    	TIM2->CCR2 = 0;										// Switch off the FAN
    	TIM3->CCR4 = 0;										// Switch off the Hot Air Gun
    	++errors;
		return false;
    }
    HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3_buff, ADC3_TEMP);
	adc_mode = ADC_TEMP;
	return true;
}

/*
 * IRQ handler
 * on TIM2 Output channel #3 to read the current through the IRON and FAN
 * also check that TIM3 counter changed driven by AC_ZERO interrupt
 * on TIM2 Output channel #4 to read the IRON and HOt Air Gun temperatures
 */

extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM2) {
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
			adcStartCurrent();
		} else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
			adcStartTemp();
		}
	}
}

/*
 * IRQ handler of ADC complete request.
 * ADC1 used to check the current through the IRON, current through the FAN and the Hot Air Gun temperature
 * 		[iron_current, fan_current, gun_temp, vrefint, internal_temp]
 * ADC3 used to check the IRON and ambient temperature
 * 		[iron_temp, iron_temp, iron_temp, iron_temp, ambient]
 */
extern "C" void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
	HAL_ADC_Stop(hadc);
	if (adc_manual) {										// Read the ADC value in setup() routine
		adc_manual = false;
		return;
	}
	if (hadc->Instance == ADC1) {
		if (TIM2->CCR1 > 1) {								// The IRON has been powered
			core.iron.updateCurrent(adc1_buff[0]);			// adc1_buff[0] is the current through the IRON
		}
		if (TIM2->CCR2 > 1) {								// The Hot Air Gun FAN has been powered
			core.hotgun.updateCurrent(adc1_buff[1]);		// adc1_buff[1] is the current through the FAN
		}
		uint16_t gun_temp	= 0; 							// adc1_buff[4..7] are the Hot Air Gun temperature
		uint16_t gun_min	= 4096;
		uint16_t gun_max	= 0;
		for (uint8_t i = 2; i <= 5; ++i) {
			gun_temp += adc1_buff[i];
			if (adc1_buff[i] > gun_max) gun_max = adc1_buff[i];
			if (adc1_buff[i] < gun_min) gun_min = adc1_buff[i];
		}
		gun_temp -= gun_min + gun_max;
		gun_temp += 1;
		gun_temp >>= 1;
		core.hotgun.updateTemp(gun_temp);
	} else if (hadc->Instance == ADC3) {					// Ambient temperature checking
		// Check the IRON temperature and calculate the required power
		uint32_t iron_temp = adc3_buff[0];
		for (uint8_t i = 1; i < 4; ++i)						// adc3_buff[0-3] is the IRON temperature
			iron_temp += adc3_buff[i];
		iron_temp += 2; iron_temp >>= 2;
		core.updateAmbient(adc3_buff[4]);					// adc3_buff[4] is ambient temperature (sensor inside T12 handle)
		uint16_t iron_power = core.iron.power(iron_temp);
		if (iron_power > max_iron_pwm)						// The required power is greater than timer period. Initialized in setup()
			iron_power = max_iron_pwm;
		TIM2->CCR1	= iron_power;
	}
	adc_mode = ADC_IDLE;
}

/*
 * IRQ handler for buzzer note period (TIM6)
 */
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3) {
		uint32_t tm		= HAL_GetTick();
		ac_sine			= gtim_last_ms + 30 > tm;			// Last TIM3 external interrupt was less than 30 ms before. 50Hz outlet interrupt period is 10 ms
		if (ac_sine && gtim_last_ms > 0) {
			gtim_period.update((tm - gtim_last_ms) * 100);
		}
		gtim_last_ms	= tm;
	} else if (htim->Instance == TIM6) {
		core.buzz.playSongCB();
	}
}

void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance != TIM3) return;
	uint16_t gun_power	= 0;								// First half of the pwr_buffer has been sent, calculate next buffer values
	if (ac_sine)
		gun_power	= core.hotgun.power();
	if (gun_power) {
		calculateGunPowerData(&gun_pwr[MAX_GUN_POWER], MAX_GUN_POWER, gun_power);
	} else {
		powerOffGun();
	}
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance != TIM3) return;
	uint16_t gun_power	= 0;								// Second half of the pwr_buffer has been sent, calculate next buffer values
	if (ac_sine)
		gun_power	= core.hotgun.power();
	if (gun_power) {
		calculateGunPowerData(gun_pwr, MAX_GUN_POWER, gun_power);
	} else {
		powerOffGun();
	}
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) 				{ }
extern "C" void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc) 	{ }
