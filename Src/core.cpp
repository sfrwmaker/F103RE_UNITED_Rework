/*
 * core.cpp
 *
 * 2024 NOV 16, v.1.00
 * 		Ported from JBC controller source code, tailored to the new hardware
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
 *  D2	- TIM3_ETR, AC zero signal read - clock source
 *  	  TIM3_CH1, Output compare (97) to calculate power to the Hot Air Gun
 *  B1	- TIM3_CH4, Hot Air Gun power [0-99]
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
 */

#include <math.h>
#include "core.h"
#include "hw.h"
#include "mode.h"
#include "work_mode.h"
#include "menu.h"
#include "vars.h"

// Activated ADC Ranks Number (hadc1.Init.NbrOfConversion)
#define ADC1_CUR 			(5)
#define ADC3_TEMP			(5)

extern ADC_HandleTypeDef	hadc1;
extern ADC_HandleTypeDef	hadc3;
extern TIM_HandleTypeDef	htim2;
extern TIM_HandleTypeDef	htim3;

extern uint8_t				usb_flash_busy;					// Defined in usbd_storage_if.c

volatile static uint32_t	errors		= 0;

typedef enum { ADC_IDLE, ADC_CURRENT, ADC_TEMP } t_ADC_mode;
volatile static t_ADC_mode	adc_mode = ADC_IDLE;
volatile static uint16_t	adc1_buff[ADC1_CUR];			// Current data: IRON, FAN, GUN temperature, VREFint, INTERNAL_temperature
volatile static uint16_t	adc3_buff[ADC3_TEMP];			// Temperature data: IRON * 4, AMBIENT
volatile static	uint32_t	tim3_cntr	= 0;				// Previous value of TIM3 counter (AC_ZERO). Using to check the TIM3 value changing
volatile static	bool		ac_sine		= false;			// Flag indicating that TIM3 is driven by AC power interrupts on AC_ZERO pin
volatile static bool		adc_manual	= false;			// Flag indicating that we have to manage ADC DMA buffer manually
static 	EMP_AVERAGE			gtim_period;					// gun timer period (ms)
static  uint16_t  			max_iron_pwm	= 0;			// Max value should be less than TIM3.CH3 value by 40. Will be initialized later
volatile static uint32_t	gtim_last_ms	= 0;			// Time when the gun timer became zero
const static	uint16_t  	max_gun_pwm		= 99;			// TIM1 period. Full power can be applied to the HOT GUN
const static	uint32_t	check_sw_period = 100;			// IRON switches check period, ms

static HW		core;										// Hardware core (including all device instances)

// MODE instances
static	MWORK			work(&core);
static	MSLCT			iselect(&core);
static	MTACT			activate(&core);
static	MCALIB			calib_auto(&core);
static	MCALIB_MANUAL	calib_manual(&core);
static	MCALMENU		calib_menu(&core, &calib_auto, &calib_manual);
static	MFAIL			fail(&core);
static	MTPID			manual_pid(&core);
static 	MAUTOPID		auto_pid(&core);
static	MENU_PID		pid_menu(&core, &manual_pid, &auto_pid);
static  MABOUT			about(&core);
static  MDEBUG			debug(&core);
static	FFORMAT			format(&core);
static	MSETUP			param_menu(&core, &pid_menu);
static	MENU_T12		t12_menu(&core, &calib_menu);
static	MENU_JBC		jbc_menu(&core, &calib_menu);
static  MENU_GUN		gun_menu(&core, &calib_manual);
static	MMENU			main_menu(&core, &iselect, &param_menu, &activate, &t12_menu, &jbc_menu, &gun_menu, &about);
static	MODE*           pMode = &work;

bool 		isACsine(void)		{ return ac_sine; 				}
uint16_t	gtimPeriod(void)	{ return gtim_period.read();	}

// Synchronize TIM2 timer to AC power. The main timer managing IRON and FAN
static uint16_t syncAC(uint16_t tim_cnt) {
	uint32_t to = HAL_GetTick() + 300;						// The timeout
	uint16_t nxt_tim3	= TIM3->CNT + 2;					// TIM3 is clocked by AC zero crossing signal, its period is 99.
	if (nxt_tim3 > 99) nxt_tim3 -= 99;
	while (HAL_GetTick() < to) {							// Prevent hang
		if (TIM3->CNT == nxt_tim3) {
			TIM2->CNT = tim_cnt;							// Synchronize TIM2 to AC power zero crossing signal
			break;
		}
	}
	// Checking the TIM2 has been synchronized
	to = HAL_GetTick() + 300;
	nxt_tim3 = TIM3->CNT + 2;
	if (nxt_tim3 > 99) nxt_tim3 -= 99;
	while (HAL_GetTick() < to) {
		if (TIM3->CNT == nxt_tim3) {
			return TIM2->CNT;
		}
	}
	return TIM2->ARR+1;										// This value is bigger than TIM2 period, the TIM2 has not been synchronized
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

	// ADC1 reads [iron_current, fan_current, gun_temp, ambient, vrefint, internal_temp]
	adc_manual = true;										// Manage DMA reading buffer manually, see HAL_ADC_ConvCpltCallback()
	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1_buff, ADC1_CUR);
	while (adc_manual) { }									// Wait for ADC readings
	uint16_t gun_temp	= adc1_buff[2];
	uint16_t vref		= adc1_buff[3];
	uint16_t t_mcu		= adc1_buff[4];

	gtim_period.length(10);
	gtim_period.reset(1000);								// Default TIM1 period, ms
	max_iron_pwm	= htim2.Instance->CCR4 - 40;			// Stop supplying power in 40 mkS before start checking IRON temperature

	CFG_STATUS cfg_init = core.init(iron_temp, gun_temp, ambient, vref, t_mcu);

	HAL_TIM_PWM_Start(&htim3, 	TIM_CHANNEL_4);				// PWM signal of Hot Air Gun
	HAL_TIM_OC_Start_IT(&htim3, TIM_CHANNEL_1);				// Calculate power of Hot Air Gun interrupt
	HAL_TIM_PWM_Start(&htim2, 	TIM_CHANNEL_1);				// PWM signal of the IRON
	HAL_TIM_PWM_Start(&htim2, 	TIM_CHANNEL_2);				// PWM signal of the FAN
	HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_3);				// Check the current through the IRON and FAN
	HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_4);				// Calculate power of the IRON

	// Setup main mode parameters: return mode, short press mode, long press mode
	work.setup(&main_menu, &iselect, &main_menu);
	iselect.setup(&work, &activate, &main_menu);
	activate.setup(&work, &work, &main_menu);
	activate.setFail(&fail);
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
	main_menu.setup(&work, &work, &work);
	about.setup(&work, &work, &debug);
	debug.setup(&work, &work, &work);
	auto_pid.setup(&work, &manual_pid, &manual_pid);
	format.setup(&work, 0, 0);

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
				fail.setMessage(MSG_EEPROM_READ);
				fail.setup(&fail, &fail, &format);			// Do not enter the main working mode
				pMode	= &fail;
				break;
			case CFG_NO_FILESYSTEM:
				fail.setMessage(MSG_FORMAT_FAILED);			// Prepare the fail message
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
	syncAC(1500);											// Synchronize TIM5 timer to AC power. Parameter is TIM5 counter value when TIM1 become zero
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
	static uint32_t AC_check_time	= 0;					// Time in ms when to check TIM3 is running
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
		TIM3->CCR2  = 0;
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
		TIM3->CCR2	= 0;
		pMode->clean();
		pMode = new_mode;
		pMode->init();
	}

	// If TIM3 counter has been changed since last check, we received AC_ZERO events from AC power
	if (HAL_GetTick() >= AC_check_time) {
		ac_sine		= (TIM3->CNT != tim3_cntr);
		tim3_cntr	= TIM3->CNT;
		AC_check_time = HAL_GetTick() + 41;					// 50Hz AC line generates 100Hz events. The pulse period is 10 ms
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
 * on TIM3 Output channel #1 to calculate required power for Hot Air Gun
 * on TIM2 Output channel #3 to read the current through the IRON and FAN
 * also check that TIM3 counter changed driven by AC_ZERO interrupt
 * on TIM2 Output channel #4 to read the IRON and HOt Air Gun temperatures
 */

extern "C" void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
		uint16_t gun_power	= core.hotgun.power();
		if (gun_power > max_gun_pwm) gun_power = max_gun_pwm;
		TIM3->CCR4	= gun_power;							// Apply Hot Air Gun power
		uint32_t n = HAL_GetTick();
		if (ac_sine && gtim_last_ms > 0) {
			gtim_period.update(n - gtim_last_ms);
		}
		gtim_last_ms = n;
	} else if (htim->Instance == TIM2) {
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
	if (adc_manual) {									// Read the ADC value in setup() routine
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
		core.hotgun.updateTemp(adc1_buff[2]);				// adc1_buff[2] is the Hot Air Gun temperature
		core.updateIntTemp(adc1_buff[3], adc1_buff[4]);		// adc1_buff[4] is vrefint, adc1_buff[5] is the MCU internal temperature
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
 * IRQ handler for buzzer note period
 */
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance != TIM6) return;
	core.buzz.playSongCB();
}

extern "C" void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) 				{ }
extern "C" void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc) 	{ }
