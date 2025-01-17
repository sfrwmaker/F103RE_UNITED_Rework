/*
 * iron_tips.h
 *
 *  Created on: 2024 DEC 24
 *      Author: Alex
 */

#ifndef IRON_TIPS_H_
#define IRON_TIPS_H_

#include <stdint.h>
#include <string>

typedef enum {TIP_NONE = 0, TIP_T12, TIP_N1, TIP_JBC, TIP_C245, TIP_HOTGUN, TIP_INVALID} TIP_TYPE_t;

#define	NO_TIP_CHUNK	255									// The flag showing that the tip was not found in the tipcal.dat
#define	tip_name_sz		(5)

/* RADIX 50 is an uppercase-only character encoding created by DEC for use on their DECsystem, PDP, and VAX computers.
 * Here the RADIX 50 is used to encode the soldering TIP names: Tip type + 5 letters grouped to 4-byte word.
 * Tip types are: None, 'T12', 'N1', 'JBC', 'C245'
 * The symbol table is:
 * 0 		- space
 * 1-26		- A-Z
 * 27		- '*'
 * 28		- '.'
 * 29		- '-'
 * 30-39	- 0-9
 */
class RADIX {
	public:
		RADIX(void)											{ }
		void		setActivated(void)						{ data[3] |= 0x40; 										}
		void		setCalibrated(void)						{ data[3] |= 0x80; 										}
		void		clearActivated(void)					{ data[3] &= ~0x40;										}
		void		clearCalibrated(void)					{ data[3] &= ~0x80;										}
		bool		isActivated(void)						{ return data[3] & 0x40; 								}
		bool		isCalibrated(void)						{ return data[3] & 0x80; 								}
		uint8_t		getCalibMask(void)						{ return data[3] >> 6;									}
		void		initEmpty(void)							{ data[0] = data[1] = data[2] = data[3] = 0;			}
		void		init(RADIX &r);
		bool		init(TIP_TYPE_t tip_type, const char name[], uint8_t size);
		bool		init(const char *full_name, uint8_t size);
		bool		isEmpty(void);
		bool		isExtraTip(void);
		uint32_t	tip(void);
		bool 		match(RADIX &r);
		std::string tipName(void);
		void		setCalibMask(uint8_t calib_mask);
		TIP_TYPE_t	type(void);
		uint32_t	word32(void);
	private:
		uint8_t 	charToRadix(const char sym);
		uint8_t		radixToChar(uint8_t code);
		uint8_t		tipTypePrefixLength(TIP_TYPE_t type);
		TIP_TYPE_t	tipType(const char *full_name, uint8_t size);
		void		bytes(uint32_t word);
		uint8_t		data[4] = {0};							// Use 4 bytes, not a uint32_t to decrease size of the TIP_TABLE
};

/*
 * This structure presents a tip record for all possible tips, loaded from tip.list.txt
 * During controller initialization phase, the buildTipTable() function creates
 * the tip list in memory of all possible tips. If the tip is calibrated, i.e. has a record
 * in the tipcal.dat file on W25Qxx flash, the tip record saves index of the tip record in this file
 */
typedef struct s_tip_table		TIP_TABLE;
struct s_tip_table {
	RADIX		tip;										// Tip name in RADIX encoding and bitmap: TIP_ACTIVE, TIP_CALIBRATED
	uint8_t		tip_index;									// The tip index in the calib.tip file
};

class TIPS {
	public:
		TIPS()												{ }
		bool			create(uint16_t size);
		bool 			initTip(uint16_t index, RADIX &tip_name);
		bool			applyCalibtationIndex(RADIX &tip_name, uint8_t calib_index);
		uint16_t		total(void)							{ return tip_count; }
		RADIX&			radix(uint16_t index);
		uint8_t			tipCalibrationIndex(uint16_t index);
		int16_t 		index(RADIX &tip_name);
		void 			clearAllCalibration(void);
	private:
		TIP_TABLE	*tip_table  = 0;						// All tips loaded from the tip_list.txt file
		uint16_t	tip_count	= 0;						// Number of tips in tip_table
		RADIX		no_tip;									// Empty tip_name used when invalid index specified
};


#endif
