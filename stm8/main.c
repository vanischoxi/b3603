/* Copyright (C) 2015 Baruch Even
 *
 * This file is part of the B3603 alternative firmware.
 *
 *  B3603 alternative firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  B3603 alternative firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with B3603 alternative firmware.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stm8s.h"
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "main.h"
#include "fixedpoint.h"
#include "uart.h"
#include "eeprom.h"
#include "outputs.h"
#include "config.h"
#include "parse.h"
#include "adc.h"
#include "buttons.h"
#include "fsm.h"

#undef DEBUG

cfg_system_t cfg_system;
cfg_output_t cfg_output;
state_t state;

inline void iwatchdog_init(void)
{
	IWDG_KR = 0xCC; // Enable IWDG
	// The default values give us about 15msec between pings
}

inline void iwatchdog_tick(void)
{
	IWDG_KR = 0xAA; // Reset the counter
}

void commit_output()
{
	output_commit(&cfg_output, &cfg_system, state.constant_current);
}

void set_name(uint8_t *name)
{
	uint8_t idx;

	for (idx = 0; name[idx] != 0; idx++) {
		if (!isprint(name[idx]))
			name[idx] = '.'; // Eliminate non-printable chars
	}

	memcpy(cfg_system.name, name, sizeof(cfg_system.name));
	cfg_system.name[sizeof(cfg_system.name)-1] = 0;

	uart_write_str("SNAME: ");
	uart_write_str(cfg_system.name);
	uart_write_str("\r\n");
}

void autocommit(void)
{
	if (cfg_system.autocommit) {
		commit_output();
	} else {
		uart_write_str("AUTOCOMMIT OFF: CHANGE PENDING ON COMMIT\r\n");
	}
}

void set_output(uint8_t *s)
{
	if (s[1] != 0)
		goto write_error;

	if (s[0] == '0') {
		cfg_system.output = 0;
		uart_write_str("OUTPUT: OFF\r\n");
	} else if (s[0] == '1') {
		cfg_system.output = 1;
		uart_write_str("OUTPUT: ON\r\n");
	} else {
		goto write_error;
	}

	autocommit();
	return;

write_error:
	uart_write_str("OUTPUT takes either 0 for OFF or 1 for ON, received: \"");
	uart_write_str(s);
	uart_write_str("\"\r\n");
}

void set_voltage(uint8_t *s, uint16_t voltage)
{
	fixed_t val = voltage;

	if (val == 0) {
		if (s == NULL) return;
		val = parse_set_value(s);
	}
	if (val == 0xFFFF)
		return;

	if ((val > CAP_VMAX) || (val < CAP_VMIN)) {
		uart_write_str("VOLTAGE OUT OF THE ALLOWED RANGE\r\n");
		return;
	}

	cfg_output.vset = val;
	autocommit();

	//Only writes the set in uart if the command came through uart (better performance)
	if (s != NULL) {
		uart_write_str("VOLTAGE: SET ");
		uart_write_millivalue(cfg_output.vset);
		uart_write_str("\r\n");
	}
}

void set_current(uint8_t *s, uint16_t current)
{
	fixed_t val = current;

	if (val == 0) {
		if (s == NULL) return;
		val = parse_set_value(s);
	}
	if (val == 0xFFFF)
		return;

	if ((val > CAP_CMAX) || (val < CAP_CMIN)) {
		uart_write_str("CURRENT OUT OF THE ALLOWED RANGE\r\n");
		return;
	}

	cfg_output.cset = val;
	autocommit();

	//Only writes the set in uart if the command came through uart (better performance)
	if (s != NULL) {
		uart_write_str("CURRENT: SET ");
		uart_write_millivalue(cfg_output.cset);
		uart_write_str("\r\n");
	}
}

void set_autocommit(uint8_t *s)
{
	if (strcmp(s, "1") == 0 || strcmp(s, "YES") == 0) {
		cfg_system.autocommit = 1;
		uart_write_str("AUTOCOMMIT: YES\r\n");
	} else if (strcmp(s, "0") == 0 || strcmp(s, "NO") == 0) {
		cfg_system.autocommit = 0;
		uart_write_str("AUTOCOMMIT: NO\r\n");
	} else {
		uart_write_str("UNKNOWN ARG. USE 1 or 0.\r\n");
	}
}

void set_calibration(const char *name, uint32_t *pval, uint8_t *s)
{
	uint32_t val = parse_uint32(s);
	if (val == 0xFFFFFFFF) {
		uart_write_str("FAILED TO PARSE");
	} else {
		*pval = val;
		uart_write_str("CALIBRATION SET ");
		uart_write_str(name);
	}
	uart_write_str("\r\n");
}

void write_str(const char *prefix, const char *val)
{
	uart_write_str(prefix);
	uart_write_str(val);
	uart_write_str("\r\n");
}

void write_onoff(const char *prefix, uint8_t on)
{
	write_str(prefix, on ? "ON" : "OFF");
}

void write_millivalue(const char *prefix, uint16_t millival)
{
	uart_write_str(prefix);
	uart_write_millivalue(millival);
	uart_write_str("\r\n");
}

void write_int(const char *prefix, uint16_t val)
{
	uart_write_str(prefix);
	uart_write_int(val);
	uart_write_str("\r\n");
}

void process_input()
{
	// Eliminate the CR/LF character
	uart_read_buf[uart_read_len-1] = 0;

	//if (strcmp(uart_read_buf, "MODEL") == 0) {
	//	uart_write_str("MODEL: " MODEL "\r\n");
	//} else if (strcmp(uart_read_buf, "VERSION") == 0) {
	//	uart_write_str("VERSION: " FW_VERSION "\r\n");
	//}
	if (strcmp(uart_read_buf, "SYSTEM") == 0) {
		uart_write_str("MODEL: " MODEL "\r\n" "VERSION: " FW_VERSION "\r\n");
		write_str("NAME: ", cfg_system.name);
		write_onoff("ONSTARTUP: ", cfg_system.default_on);
		write_onoff("AUTOCOMMIT: ", cfg_system.autocommit);
	} else if (strcmp(uart_read_buf, "CALIBRATION") == 0) {
		uart_write_str("CALIBRATE VIN ADC: ");
		uart_write_fixed_point(cfg_system.vin_adc.a);
		uart_write_ch('/');
		uart_write_fixed_point(cfg_system.vin_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE VOUT ADC: ");
		uart_write_fixed_point(cfg_system.vout_adc.a);
		uart_write_ch('/');
		uart_write_fixed_point(cfg_system.vout_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE COUT ADC: ");
		uart_write_fixed_point(cfg_system.cout_adc.a);
		uart_write_ch('/');
		uart_write_fixed_point(cfg_system.cout_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE VOUT PWM: ");
		uart_write_fixed_point(cfg_system.vout_pwm.a);
		uart_write_ch('/');
		uart_write_fixed_point(cfg_system.vout_pwm.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE COUT PWM: ");
		uart_write_fixed_point(cfg_system.cout_pwm.a);
		uart_write_ch('/');
		uart_write_fixed_point(cfg_system.cout_pwm.b);
		uart_write_str("\r\n");
/*	} else if (strcmp(uart_read_buf, "RCALIBRATION") == 0) {
		uart_write_str("CALIBRATE VIN ADC: ");
		uart_write_int32(cfg_system.vin_adc.a);
		uart_write_ch('/');
		uart_write_int32(cfg_system.vin_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE VOUT ADC: ");
		uart_write_int32(cfg_system.vout_adc.a);
		uart_write_ch('/');
		uart_write_int32(cfg_system.vout_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE COUT ADC: ");
		uart_write_int32(cfg_system.cout_adc.a);
		uart_write_ch('/');
		uart_write_int32(cfg_system.cout_adc.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE VOUT PWM: ");
		uart_write_int32(cfg_system.vout_pwm.a);
		uart_write_ch('/');
		uart_write_int32(cfg_system.vout_pwm.b);
		uart_write_str("\r\n");
		uart_write_str("CALIBRATE COUT PWM: ");
		uart_write_int32(cfg_system.cout_pwm.a);
		uart_write_ch('/');
		uart_write_int32(cfg_system.cout_pwm.b);
		uart_write_str("\r\n");
*/	} else if (strcmp(uart_read_buf, "LIMITS") == 0) {
		uart_write_str("LIMITS:\r\n");
		write_millivalue("VMIN: ", CAP_VMIN);
		write_millivalue("VMAX: ", CAP_VMAX);
		write_millivalue("VSTEP: ", CAP_VSTEP);
		write_millivalue("CMIN: ", CAP_CMIN);
		write_millivalue("CMAX: ", CAP_CMAX);
		write_millivalue("CSTEP: ", CAP_CSTEP);
	} else if (strcmp(uart_read_buf, "CONFIG") == 0) {
		uart_write_str("CONFIG:\r\n");
		write_onoff("OUTPUT: ", cfg_system.output);
		write_millivalue("VSET: ", cfg_output.vset);
		write_millivalue("CSET: ", cfg_output.cset);
	} else if (strcmp(uart_read_buf, "STATUS") == 0) {
		uart_write_str("STATUS:\r\n");
		write_onoff("OUTPUT: ", cfg_system.output);
		write_millivalue("VIN: ", state.vin);
		write_millivalue("VOUT: ", state.vout);
		write_millivalue("COUT: ", state.cout);
		write_str("CONSTANT: ", state.constant_current ? "CURRENT" : "VOLTAGE");
	} else if (strcmp(uart_read_buf, "RSTATUS") == 0) {
		uart_write_str("RSTATUS:\r\n");
		write_int("VIN ADC: ", state.vin_raw);
		write_int("VOUT ADC: ", state.vout_raw);
		write_int("COUT ADC: ", state.cout_raw);
	} else if (strcmp(uart_read_buf, "COMMIT") == 0) {
		commit_output();
	} else if (strcmp(uart_read_buf, "SAVE") == 0) {
		uart_write_str(((config_save_system(&cfg_system) == 0) ||
				(config_save_output(&cfg_output) == 0)) ?
				"ERROR SAVING\r\n" : "SAVED\r\n");
	} else if (strcmp(uart_read_buf, "LOAD") == 0) {
		config_load_system(&cfg_system);
		config_load_output(&cfg_output);
		autocommit();
	} else if (strcmp(uart_read_buf, "RESTORE") == 0) {
		config_default_system(&cfg_system);
		config_default_output(&cfg_output);
		autocommit();
#if DEBUG
	} else if (strcmp(uart_read_buf, "STUCK") == 0) {
		// Allows debugging of the IWDG feature
		uart_write_str("STUCK\r\n");
		uart_write_flush();
		while(1); // Induce watchdog reset
#endif
	} else {
		// Process commands with arguments
		uint8_t idx;
		uint8_t space_found = 0;

		for (idx = 0; idx < uart_read_len; idx++) {
			if (uart_read_buf[idx] == ' ') {
				uart_read_buf[idx] = 0;
				space_found = 1;
				break;
			}
		}

		if (space_found) {
			if (strcmp(uart_read_buf, "SNAME") == 0) {
				set_name(uart_read_buf + idx + 1);
			} else if (strcmp(uart_read_buf, "OUTPUT") == 0) {
				set_output(uart_read_buf + idx + 1);
			} else if (strcmp(uart_read_buf, "VOLTAGE") == 0) {
				set_voltage(uart_read_buf + idx + 1, 0);
			} else if (strcmp(uart_read_buf, "CURRENT") == 0) {
				set_current(uart_read_buf + idx + 1, 0);
			} else if (strcmp(uart_read_buf, "AUTOCOMMIT") == 0) {
				set_autocommit(uart_read_buf + idx + 1);
			} else if (strcmp(uart_read_buf, "CALVINADCA") == 0) {
				set_calibration("VIN ADC A", &cfg_system.vin_adc.a, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALVINADCB") == 0) {
				set_calibration("VIN ADC B", &cfg_system.vin_adc.b, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALVOUTADCA") == 0) {
				set_calibration("VOUT ADC A", &cfg_system.vout_adc.a, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALVOUTADCB") == 0) {
				set_calibration("VOUT ADC B", &cfg_system.vout_adc.b, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALVOUTPWMA") == 0) {
				set_calibration("VOUT PWM A", &cfg_system.vout_pwm.a, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALVOUTPWMB") == 0) {
				set_calibration("VOUT PWM B", &cfg_system.vout_pwm.b, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALCOUTADCA") == 0) {
				set_calibration("COUT ADC A", &cfg_system.cout_adc.a, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALCOUTADCB") == 0) {
				set_calibration("COUT ADC B", &cfg_system.cout_adc.b, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALCOUTPWMA") == 0) {
				set_calibration("COUT PWM A", &cfg_system.cout_pwm.a, uart_read_buf+idx+1);
			} else if (strcmp(uart_read_buf, "CALCOUTPWMB") == 0) {
				set_calibration("COUT PWM B", &cfg_system.cout_pwm.b, uart_read_buf+idx+1);
			} else {
				uart_write_str("UNKNOWN COMMAND!\r\n");
			}
		} else {
			uart_write_str("UNKNOWN COMMAND\r\n");
		}
	}
	uart_write_str("DONE\r\n");

	uart_read_len = 0;
	read_newline = 0;
}

inline void clk_init()
{
	CLK_CKDIVR = 0x00; // Set the frequency to 16 MHz
}

inline void pinout_init()
{
	// PA1 is 74HC595 SHCP, output
	// PA2 is 74HC595 STCP, output
	// PA3 is CV/CC leds, output (& input to disable)
	PA_ODR = 0;
	PA_DDR = (1<<1) | (1<<2);
	PA_CR1 = (1<<1) | (1<<2) | (1<<3);
	PA_CR2 = (1<<1) | (1<<2) | (1<<3);

	// PB4 is Enable control, output
	// PB5 is CV/CC sense, input
	PB_ODR = (1<<4); // For safety we start with off-state
	PB_DDR = (1<<4);
	PB_CR1 = (1<<4);
	PB_CR2 = 0;

	// PC3 is unknown, input
	// PC4 is Iout sense, input adc, AIN2
	// PC5 is Vout control, output
	// PC6 is Iout control, output
	// PC7 is Button 1, input
	PC_ODR = 0;
	PC_DDR = (1<<5) | (1<<6);
	PC_CR1 = (1<<7); // For the button
	PC_CR2 = (1<<5) | (1<<6);

	// PD1 is Button 2, input
	// PD2 is Vout sense, input adc, AIN3
	// PD3 is Vin sense, input adc, AIN4
	// PD4 is 74HC595 DS, output
	PD_DDR = (1<<4);
	PD_CR1 = (1<<1) | (1<<4); // For the button
	PD_CR2 = (1<<4);
}

void config_load(void)
{
	config_load_system(&cfg_system);
	config_load_output(&cfg_output);

	if (cfg_system.default_on)
		cfg_system.output = 1;
	else
		cfg_system.output = 0;

#if DEBUG
	state.pc3 = 1;
#endif
}

void read_state(void)
{
	uint8_t tmp;

#if DEBUG
	tmp = (PC_IDR & (1<<3)) ? 1 : 0;
	if (state.pc3 != tmp) {
		uart_write_str("PC3 is now ");
		uart_write_ch('0' + tmp);
		uart_write_str("\r\n");
		state.pc3 = tmp;
	}
#endif

	tmp = (PB_IDR & (1<<5)) ? 1 : 0;
	if (state.constant_current != tmp) {
		state.constant_current = tmp;
		output_check_state(&cfg_system, state.constant_current);
	}

	if (adc_ready()) {
		uint16_t val = adc_read();
		uint8_t ch = adc_channel();

		switch (ch) {
			case 2:
				state.cout_raw = val;
				// Calculation: val * cal_cout_a * 3.3 / 1024 - cal_cout_b
				state.cout = adc_to_volt(val, &cfg_system.cout_adc);
				ch = 3;
				break;
			case 3:
				state.vout_raw = val;
				// Calculation: val * cal_vout_a * 3.3 / 1024 - cal_vout_b
				state.vout = adc_to_volt(val, &cfg_system.vout_adc);
				ch = 4;
				break;
			case 4:
				state.vin_raw = val;
				// Calculation: val * cal_vin * 3.3 / 1024
				state.vin = adc_to_volt(val, &cfg_system.vin_adc);
				ch = 2;
				break;
		}

		adc_start(ch);
	}
}

void ensure_afr0_set(void)
{
	if ((OPT2 & 1) == 0) {
		uart_flush_writes();
		if (eeprom_set_afr0()) {
			uart_write_str("AFR0 set, reseting the unit\r\n");
			uart_flush_writes();
			iwatchdog_init();
			while (1); // Force a reset in a few msec
		}
		else {
			uart_write_str("AFR0 not set and programming failed!\r\n");
		}
	}
}

int main()
{
	unsigned long i = 0;
	button_t button = BUTTON_NONE;

	pinout_init();
	clk_init();
	uart_init();
	pwm_init();
	adc_init();

	config_load();

	uart_write_str("\r\n" MODEL " starting: Version " FW_VERSION "\r\n");

	ensure_afr0_set();

	iwatchdog_init();
	adc_start(4);
	commit_output();

	do {
		iwatchdog_tick();
		read_state();
		display_refresh();
		button=read_buttons();
		process_fsm(button, &cfg_system, &cfg_output, &state);
		uart_drive();
		if (read_newline) {
			process_input();
		}
	} while(1);
}
