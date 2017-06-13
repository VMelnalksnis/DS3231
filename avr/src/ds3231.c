/*
MIT License

Copyright (c) 2017 Valters Melnalksnis

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**@file ds3231.c
 *
 */

/*
 * DS3231 register map:
 * - 0x00 seconds
 * - 0x01 minutes
 * - 0x02 hours
 * - 0x03 day of the week
 * - 0x04 day of the month
 * - 0x05 MSB century; month
 * - 0x06 year
 * - 0x07 MSB A1M1; alarm 1 seconds
 * - 0x08 MSB A1M2; alarm 1 minutes
 * - 0x09 MSB A1M3; alarm 1 hours
 * - 0x0A Bit7 A1M4; Bit6 WDay/Mday (1/0); alarm 1 day
 * - 0x0B MSB A2M2; alarm 2 minutes
 * - 0x0C MSB A2M3; alarm 2 hours
 * - 0x0D Bit7 A2M4; Bit6 Wday/Mday (1/0); alarm 2 day
 * - 0x0E control:
 *  - EOSC    - enable oscillator
 *  - BBSQW   - battery-backed square-wave enable
 *  - CONV    - convert temperature
 *  - NA      - not applicable
 *  - NA      - not applicable
 *  - INTCN   - interrupt control
 *  - A2IE    - alarm 2 interrupt enable
 *  - A1IE    - alarm 1 interrupt enable
 * - 0x0F status:
 *  - OSF     - oscillator stop flag
 *  - 0       - unused
 *  - 0       - unused
 *  - 0       - unused
 *  - EN32KHZ - enable 32.768 kHz output
 *  - BSY     - busy
 *  - A2F     - alarm 2 flag
 *  - A1F     - alarm 1 flag
 * - 0x10 aging offset (signed)
 * - 0x11 temperature MSB (signed integer)
 * - 0x12 temperature LSB in two MSB bits (1/4 of a degree)
 */

#include <avr/io.h>
#include "ds3231.h"
#include "twi.h"

#define READ_ADD    0xD1                         //!< The slave address of DS3231 with the LSB set to 1.
#define WRITE_ADD   0xD0                         //!< The slave address of DS3231 with the LSB set to 0.
#define SECDR       0x00                         //!< Address of the "Seconds" register.
#define AL1DR       0X07                         //!< Address of the "Alarm 1 seconds" register.
#define AL2DR       0x0B                         //!< Address of the "Alarm 2 minutes" register.
#define CTRDR       0x0E                         //!< Address of the "Control" register.
#define STSDR       0x0F                         //!< Address of the "Status" register.
#define AGODR       0x10                         //!< Address of the "Aging Offset" register.
#define TMPDR       0x11                         //!< Address of the "Temperature MSB" register.

struct time _time;

/**Converts a decimal value to a binary coded decimal value.
 *
 * @param[in]    d           Decimal value to convert.
 *
 * @return                   Returns a binary coded decimal value of d.
 */
uint8_t dec2bcd(uint8_t d)
{
	return ((d / 10 * 16) + (d % 10));
}

/**Converts a binary coded decimal value to a decimal value.
 *
 * @param[in]    d           Binary coded decimal value to convert.
 *
 * @return                   Returns a decimal value of b.
 */
uint8_t bcd2dec(uint8_t b)
{
	return ((b / 16 * 10) + (b % 16));
}

uint8_t ds3231_get_time(struct time* time_)
{
	uint8_t msgBuf[8];
	uint8_t century;

	// Write the address of the first register to read ("Seconds")
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = SECDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read register 0x00..0x06
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 8))
	{
		// Handle transmission error
		return (false);
	}

	// Update stored time
	_time.sec = bcd2dec(msgBuf[1]);
	_time.min = bcd2dec(msgBuf[2]);
	_time.hour = bcd2dec(msgBuf[3]);
	_time.wday = bcd2dec(msgBuf[4]);
	_time.mday = bcd2dec(msgBuf[5]);
	_time.mon = bcd2dec(msgBuf[6]) & 0x1F;       // Month data is stored in Bit4..0
	century = (msgBuf[6] & 0x80) >> 7;           // Century data is stored in Bit7
	_time.year = (century == 1) ? 2000 + bcd2dec(msgBuf[7]) : 1900 + bcd2dec(msgBuf[7]);

	// Deal with 12-hour mode
	if(_time.hour == 0)
	{
		_time.twelveHour = 0;
		_time.am = true;
	}
	else if(_time.hour < 12)
	{
		_time.twelveHour = _time.hour;
		_time.am = true;
	}
	else
	{
		_time.twelveHour = _time.hour - 12;
		_time.am = false;
	}

	*time_ = _time;

	return (true);
}

uint8_t ds3231_get_time_s(uint8_t* hour, uint8_t* min, uint8_t* sec)
{
	uint8_t msgBuf[4];

	// Write the address of the first register
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = SECDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the registers 0x00..0x02
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 4))
	{
		// Handle transmission error
		return (false);
	}

	if (sec)  *sec = bcd2dec(msgBuf[1]);
	if (min)  *min = bcd2dec(msgBuf[2]);
	if (hour) *hour = bcd2dec(msgBuf[3]);

	return (true);
}

uint8_t ds3231_set_time(struct time* time_)
{
	uint8_t msgBuf[9];
	uint8_t century;

	if (time_->year > 2000)
	{
		century = 0x80;
		time_->year = time_->year - 2000;
	}
	else
	{
		century = 0x00;
		time_->year = time_->year - 1900;
	}

	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = SECDR;
	msgBuf[2] = dec2bcd(time_->sec);
	msgBuf[3] = dec2bcd(time_->min);
	msgBuf[4] = dec2bcd(time_->hour);
	msgBuf[5] = dec2bcd(time_->wday);
	msgBuf[6] = dec2bcd(time_->mday);
	msgBuf[7] = dec2bcd(time_->mon) + century;
	msgBuf[8] = dec2bcd(time_->year);

	if (TWI_start_transceiver_with_data(msgBuf, 9))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_set_time_s(uint8_t hour, uint8_t min, uint8_t sec)
{
	uint8_t msgBuf[5];

	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = SECDR;
	msgBuf[2] = dec2bcd(sec);
	msgBuf[3] = dec2bcd(min);
	msgBuf[4] = dec2bcd(hour);

	if (TWI_start_transceiver_with_data(msgBuf, 5))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_get_temp_int(int8_t* i, uint8_t* f)
{
	uint8_t msgBuf[3];

	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = TMPDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 3))
	{
		// Handle transmission error
		return (false);
	}

	*i = msgBuf[1];
	*f = (msgBuf[2] >> 6);

	return (true);
}

uint8_t ds3231_force_temp_conversion(uint8_t block)
{
	// TODO

	return (true);
}

uint8_t ds3231_SQW_enable(bool enable)
{
	uint8_t msgBuf[3];

	// Write the address of the "Control" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = CTRDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the "Control" register
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	if (enable)
	{
		msgBuf[1] |= 0x40;                       // Enable battery-backed square-wave oscillator
		msgBuf[1] &= 0xFB;                       // Disable alarm interrupts
	}
	else
	{
		msgBuf[1] &= 0xBF;                       // Disable batter-backed square=wave oscillator
	}

	// Write the new settings to the "Control" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[2] = msgBuf[1];
	msgBuf[1] = CTRDR;
	if (TWI_start_transceiver_with_data(msgBuf, 3))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_osc32kHz_enable(bool enable)
{
	uint8_t msgBuf[3];

	// Write the address of the "Status" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = STSDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the "Status" register
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	if (enable)
	{
		msgBuf[1] |= 0x08;                       // Enable 32 kHz oscillator
	}
	else
	{
		msgBuf[1] &= 0xF7;                       // Disable 32 kHz oscillator
	}

	// Write the new settings to the "Status" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[2] = msgBuf[1];
	msgBuf[1] = STSDR;
	if (TWI_start_transceiver_with_data(msgBuf, 3))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_reset_alarm(uint8_t alarm)
{
	uint8_t msgBuf[(alarm == ALARM_1) ? 6 : 5];

	msgBuf[0] = WRITE_ADD;
	if (alarm == ALARM_1)
	{
		msgBuf[1] = AL1DR;
		msgBuf[2] = 0x00;
		msgBuf[3] = 0x00;
		msgBuf[4] = 0x00;
		msgBuf[5] = 0x00;
	}
	else
	{
		msgBuf[1] = AL2DR;
		msgBuf[2] = 0x00;
		msgBuf[3] = 0x00;
		msgBuf[4] = 0x00;
	}

	if (TWI_start_transceiver_with_data(msgBuf, (alarm == ALARM_1) ? 6 : 5))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_set_alarm(struct time* time_, uint8_t alarm, uint8_t mode, bool intrpt)
{
	// TODO

	return (true);
}

uint8_t ds3231_set_alarm_s(uint8_t day, uint8_t hour, uint8_t min, uint8_t sec, uint8_t alarm, uint8_t mode, bool intrpt)
{
#ifdef PARAM_VERIFICATION
	if (mode == ALARM_WDAY_M && day > 7)
	{

		return (false);
	}
	if (mode == ALARM_MDAY_M && day > 31)
	{

		return (false);
	}
	if (hour > 23)
	{

		return (false);
	}
	if (min > 59)
	{

		return (false);
	}
	if (sec > 59)
	{

		return (false);
	}
	if (alarm > 1)
	{

		return (false);
	}
	if (intrpt > 1)
	{

		return (false);
	}
#endif
	uint8_t msgBuf[(alarm == ALARM_1) ? 6 : 5];

	// Write the address of the "Control" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = CTRDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the "Control" register
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	if (intrpt == true)
	{
		msgBuf[1] |= 1 << alarm;                 // Enable interrupt
	}
	else
	{
		msgBuf[1] &= ~(1 << alarm);              // Disable interrupt
	}

	// Write the new settings to the "Control" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[2] = msgBuf[1];
	msgBuf[1] = CTRDR;
	if (TWI_start_transceiver_with_data(msgBuf, 3))
	{
		// Handle transmission error
		return (false);
	}

	// Write the alarm time
	msgBuf[0] = WRITE_ADD;
	if (alarm == ALARM_1)
	{
		msgBuf[1] = AL1DR;
		msgBuf[2] = dec2bcd(sec)	| ((mode == ALARM_SEC)		? 0x80 : 0x00);
		msgBuf[3] = dec2bcd(min)	| ((mode <= ALARM_SEC_M)	? 0x80 : 0x00);
		msgBuf[4] = dec2bcd(hour)	| ((mode <= ALARM_MIN_M)	? 0x80 : 0x00);
		msgBuf[5] = dec2bcd(day)	| ((mode <= ALARM_HOUR_M)	? 0x80 : 0x00)
									| ((mode == ALARM_WDAY_M)	? 0x40 : 0x00);
	}
	else
	{
		msgBuf[1] = AL2DR;
		msgBuf[2] = dec2bcd(min)	| ((mode == ALARM_MIN)		? 0x80 : 0x00);
		msgBuf[3] = dec2bcd(hour)	| ((mode <= ALARM_MIN_M)	? 0x80 : 0x00);
		msgBuf[4] = dec2bcd(day)	| ((mode <= ALARM_HOUR_M)	? 0x80 : 0x00)
									| ((mode == ALARM_WDAY_M)	? 0x40 : 0x00);
	}
	if (TWI_start_transceiver_with_data(msgBuf, (alarm == ALARM_1) ? 6 : 5))
	{
		// Handle transmission error
		return (false);
	}

	return (true);
}

uint8_t ds3231_get_alarm(struct time* time_, uint8_t alarm, uint8_t* mode, bool* intrpt)
{
	// TODO

	return (true);
}

uint8_t ds3231_get_alarm_s(uint8_t* day, uint8_t* hour, uint8_t* min, uint8_t* sec, uint8_t alarm, uint8_t* mode, bool* intrpt)
{
#ifdef PARAM_VERIFICATION
	if (alarm > 1)
	{

		return (false);
	}
#endif
	uint8_t msgBuf[(alarm == ALARM_1) ? 5 : 4];

	// Write the "Control" register address
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = CTRDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the "Control" register
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	*intrpt = (msgBuf[1] & ~(1 << alarm));       // Get the "Alarm Enabled" bit

	// Write the address of the first register of the selected alarm
	msgBuf[0] = WRITE_ADD;
	msgBuf[1] = (alarm == ALARM_1) ? AL1DR : AL2DR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	// Read the registers of the selected alarm
	msgBuf[0] = READ_ADD;
	if (TWI_start_transceiver_with_data(msgBuf, (alarm == ALARM_1) ? 5 : 4))
	{
		// Handle transmission error
		return (false);
	}

	if (alarm == ALARM_1)
	{
		*sec = bcd2dec(msgBuf[1] & 0x7F);
		*min = bcd2dec(msgBuf[2] & 0x7F);
		*hour = bcd2dec(msgBuf[3] & 0x7F);
		*day = bcd2dec(msgBuf[4] & (((msgBuf[4] & 0x40) == 0) ? 0x3F : 0x0F));
		if ((msgBuf[4] & 0x80) == 0)
		{
			*mode = ((msgBuf[4] & 0x40) == 0) ? ALARM_MDAY_M : ALARM_WDAY_M;
		}
		else if ((msgBuf[3] & 0x80) == 0)
		{
			*mode = ALARM_HOUR_M;
		}
		else if ((msgBuf[2] & 0x80) == 0)
		{
			*mode = ALARM_MIN_M;
		}
		else if ((msgBuf[1] & 0x80) == 0)
		{
			*mode = ALARM_SEC_M;
		}
		else
		{
			*mode = ALARM_SEC;
		}
	}
	else
	{
		*min = bcd2dec(msgBuf[1] & 0x7F);
		*hour = bcd2dec(msgBuf[2] & 0x7F);
		*day = bcd2dec(msgBuf[3] & (((msgBuf[3] & 0x40) == 0) ? 0x3F : 0x0F));
		if ((msgBuf[3] & 0x80) == 0)
		{
			*mode = ((msgBuf[3] & 0x40) == 0) ? ALARM_MDAY_M : ALARM_WDAY_M;
		}
		else if ((msgBuf[2] & 0x80) == 0)
		{
			*mode = ALARM_HOUR_M;
		}
		else if ((msgBuf[1] & 0x80) == 0)
		{
			*mode = ALARM_MIN_M;
		}
		else
		{
			*mode = ALARM_MIN;
		}
	}

	return (true);
}

uint8_t ds3231_check_alarm(bool* active, uint8_t alarm)
{
#ifdef PARAM_VERIFICATION
	if (alarm > 1)
	{

		return (false);
	}
#endif
	uint8_t msgBuf[2];

	// Write the address of the "Status" register
	msgBuf[0] = WRITE_ADD;
	msgBuf[0] = STSDR;
	if (TWI_start_transceiver_with_data(msgBuf, 2))
	{
		// Handle transmission error
		return (false);
	}

	*active = (msgBuf[1] & (1 << alarm));

	return (true);
}