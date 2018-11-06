#ifndef MAIN_H
#define MAIN_H

#include "display.h"

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define FW_VERSION "2.0.1"
#define MODEL "B3606"

#define CAP_VMIN 10 // 10mV
#define CAP_VMAX 36000 // 36 V
#define CAP_VSTEP 10 // 10mV

#define CAP_CMIN 1 // 1 mA
#define CAP_CMAX 6000 // 6 A
#define CAP_CSTEP 1 // 1 mA

void set_output(uint8_t *s);
void set_voltage(uint8_t *s, uint16_t voltage);
void set_current(uint8_t *s, uint16_t current);

#endif //MAIN_H
