/*
 * iron_tips.cpp
 *
 *  Created on: 15 aug. 2019.
 *      Author: Alex
 *
 *  2024 AUG 14
 *  2024 OCT 05
 *  	Updated JBC tip list
 */

#include "iron_tips.h"

/* RADIX 50 is an uppercase-only character encoding created by DEC for use on their DECsystem, PDP, and VAX computers.
 * Here the RADIX 50 is used to encode the soldering TIP names: Tip type + 5 letters grouped to 4-byte word.
 * Tip types are: None, 'T12', 'N1', 'JBC', 'C245'
 * The symbol table is:
 * 0 		- space
 * 1-26		- A-Z
 * 27		- $
 * 28		- .
 * 29		- %
 * 30-39	- 0-9
 */

#define TIP_TYPE_NUM (5)

static const uint8_t	tip_type[TIP_TYPE_NUM][4]	= {
		{'T',  '1',   '2',   '\0'},
		{'N',  '1',   '\0',  '\0'},
		{'J',  'B',   'C',   '\0'},
		{'C',  '2',   '4',   '5' },
		{'H',  'O',   'T',   '\0'}
};
static const uint8_t	extra_symbols[3]	= {'*', '.', '-'};

bool RADIX::init(TIP_TYPE_t tip_type, const char name[], uint8_t size) {
	uint32_t d = (uint32_t)tip_type;							// First generate a 32-bits word, then split it into 4 bytes
	for (uint8_t i = 0; i < 5; ++i) {
		d *= 40;
		uint8_t next = 0;
		if (i < size) {
			next = charToRadix(name[i]);
			if (next >= 40)
				return false;
		}
		d += next;
	}
	d &= 0x3FFFFFFF;
	bytes(d);													// Convert to data[4] bytes
	return true;
}

bool RADIX::init(const char *full_name, uint8_t size) {
	TIP_TYPE_t t_type = tipType(full_name, size);
	if (t_type == TIP_INVALID)
		return false;
	uint8_t s = 0;
	if (t_type != TIP_NONE) {
		for ( ; s < size; ++s) {
			if (full_name[s] == '-') break;
		}
		if (s > 0 && s < size) {								// Tip prefix found
			s++;												// Skip hyphen sign
		}
	}
	return init(t_type, &full_name[s], size-s);
}

void RADIX::init(RADIX &r) {
	data[0] = r.data[0];
	data[1] = r.data[1];
	data[2] = r.data[2];
	data[3] = r.data[3];
}

bool RADIX::isEmpty(void) {
	if (data[0]) return false;
	if (data[1]) return false;
	if (data[2]) return false;
	return ((data[3] & 0x3F) == 0);
}

bool RADIX::isExtraTip(void) {
	return (type() == TIP_NONE);

}

uint32_t RADIX::tip(void) {
	uint32_t d = word32();
	return d & 0x3FFFFFFF;
}

bool RADIX::match(RADIX &r) {
	if (data[0] != r.data[0]) return false;
	if (data[1] != r.data[1]) return false;
	if (data[2] != r.data[2]) return false;
	return ((data[3] & 0x3F) == (r.data[3] & 0x3F));
}

std::string RADIX::tipName(void) {
	uint8_t name[11];
	uint8_t i = 11;
	name[--i] = '\0';
	uint32_t rest = word32() & 0x3FFFFFFF;
	while (i > 5) {
		uint8_t code = rest % 40;
		rest /= 40;
		name[--i] = radixToChar(code);
	}
	if (rest != 0 && rest <= TIP_TYPE_NUM) {					// Add some prefix
		name[--i] = '-';
		uint8_t type_length = tipTypePrefixLength((TIP_TYPE_t)rest);
		i -= type_length;
		for (uint8_t j = 0; j < type_length; ++j)
			name[i+j] = tip_type[rest-1][j];
	}
	for (uint8_t k = 9; k > i; --k) {							// Convert spaces at the end to '\0'
		if (name[k] != ' ') break;
		name[k] = '\0';
	}
	std::string res((const char *)&name[i]);
	return res;
}

void RADIX::setCalibMask(uint8_t calib_mask) {
	uint32_t m = calib_mask & 3;
	m <<= 6;
	data[3] &= 0x3F;
	data[3] |= m;
}

TIP_TYPE_t RADIX::type(void) {
	uint32_t res = word32() & 0x3FFFFFFF;						// Clear TIP_ACTIVE and TIP_CALIBRATED bits
	res /= 102400000;											// 40^5
	return (TIP_TYPE_t)res;
}

uint8_t RADIX::charToRadix(const char sym) {
	if (sym == ' ' || sym == '\0')
		return 0;
	if (sym >= 'A' && sym <= 'Z')
		return (uint8_t)sym - 'A' + 1;
	if (sym >= 'a' && sym <= 'z')
		return (uint8_t)sym - 'a' + 1;
	if (sym >= '0' && sym <= '9')
		return (uint8_t)sym - '0' + 30;
	for (uint8_t i = 0; i < 3; ++i) {
		if (sym == extra_symbols[i]) {
			return i + 27;
		}
	}
	return 255;													// Invalid character
}

uint8_t RADIX::radixToChar(uint8_t code) {
	if (code == 0)
		return ' ';
	if (code < 27)
		return code - 1 + 'A';
	if (code < 30)
		return extra_symbols[code-27];
	return code - 30 + '0';
}

uint8_t	RADIX::tipTypePrefixLength(TIP_TYPE_t type) {
	if (type == TIP_NONE)
		return 0;
	uint8_t indx = (uint8_t)type - 1;
	uint8_t length = 4;
	for (uint8_t i = 0; i < 4; ++i)
		if (tip_type[indx][i] == 0)
			--length;
	return length;
}

TIP_TYPE_t RADIX::tipType(const char *full_name, uint8_t size) {
	uint8_t s = 0;
	for ( ; s < size; ++s) {
		if (full_name[s] == '-') break;
	}
	if (s > 0 && s < size) {								// Tip prefix found
		for (uint8_t t = 0; t < TIP_TYPE_NUM; ++t) {
			bool match = true;
			for (uint8_t i = 0; i < 4; ++i) {
				if (full_name[i] != tip_type[t][i] && i < s) {
					match = false;
					break;
				}
			}
			if (match) {
				return (TIP_TYPE_t)(t+1);
			}
		}
		return TIP_INVALID;
	}
	return TIP_NONE;
}

uint32_t RADIX::word32(void) {
	return data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0];
}

void RADIX::bytes(uint32_t word) {
	data[0] = word & 0xFF;
	data[1] = word >> 8;
	data[2] = word >> 16;
	data[3] = word >> 24;
}

bool TIPS::create(uint16_t size) {
	tip_table = (TIP_TABLE *)malloc(size * sizeof(TIP_TABLE));
	if (tip_table) {
		tip_count = size;
		return true;
	}
	return false;
}

bool TIPS::initTip(uint16_t index, RADIX &tip_name) {
	if (index >= tip_count) return false;
	tip_table[index].tip_index = NO_TIP_CHUNK;
	tip_table[index].tip.init(tip_name);
	return true;
}

bool TIPS::applyCalibtationIndex(RADIX &tip_name, uint8_t calib_index) {
	int16_t indx = index(tip_name);
	if (indx < 0) return false;
	uint8_t calib_mask = tip_name.getCalibMask();
	tip_table[indx].tip_index = calib_index;
	tip_table[indx].tip.setCalibMask(calib_mask);
	return true;
}

void TIPS::clearAllCalibration(void) {
	for (uint16_t i = 0; i < tip_count; ++i) {
		tip_table[i].tip_index = NO_TIP_CHUNK;
		tip_table[i].tip.setCalibMask(0);
	}
}

RADIX& TIPS::radix(uint16_t index) {
	if (index < tip_count) {
		return tip_table[index].tip;
	} else {
		return no_tip;
	}
}

uint8_t	TIPS::tipCalibrationIndex(uint16_t index) {
	if (index >= tip_count)
		return NO_TIP_CHUNK;
	return tip_table[index].tip_index;
}

int16_t TIPS::index(RADIX &tip_name) {
	for (uint16_t i = 0; i < tip_count; ++i) {
		if (tip_table[i].tip.match(tip_name)) {
			return i;
		}
	}
	return -1;
}
