/*
 * core.h
 *
 *  2024 NOV 16
 *  	Ported from JBC controller source code, tailored to the new hardware
 */

#ifndef CORE_H_
#define CORE_H_

#include <stdbool.h>
#include "main.h"

// Forward function declaration
bool 		isACsine(void);
uint16_t	gtimPeriod(void);

#ifdef __cplusplus
extern "C" {
#endif

void setup(void);
void loop(void);

#ifdef __cplusplus
}
#endif

#endif
