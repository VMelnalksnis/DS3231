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

#include <avr/io.h>
#include <stdbool.h>

// Used to determine which alarm to work with
#define ALARM_1         0	//!< Select alarm 1.
#define ALARM_2         1	//!< Select alarm 2.

// Used to set alarm mode. Note that ALARM_2 does not track seconds
#define ALARM_SEC       0	//!< Alarm once a second (ALARM_1 only).
#define ALARM_SEC_M     1	//!< Alarm when seconds match (ALARM_1 only).
#define ALARM_MIN_M     2	//!< Alarm when minutes and seconds match.
#define ALARM_HOUR_M    3	//!< Alarm when hours, minutes and seconds match.
#define ALARM_MDAY_M    4	//!< Alarm when date, hours, minutes and seconds match.
#define ALARM_WDAY_M    5	//!< Alarm when day, hours, minutes and seconds match.
#define ALARM_MIN       6	//!< Alarm once a minute (at 00 seconds) (ALARM_2 only).

/**Time structure.
 *
 * Time is stored and in both 24-hour and 12-hour modes,
 * and is updated when ds3231_get_time is called.
 *
 * When setting time and alarms, 24-hour time is used.
 *
 * If you run your clock in 12-hour mode:
 * - set time hour to store in twevleHour amd set am to true or false;
 * - call ds3231_12h_translate() (this will put the correct value in hour);
 * - call ds3231_set_alarm() or ds3231_set_time().
 *
 * Note that ds3231_set_time(), ds3231_set_alarm_s(), ds3231_get_time_s()
 * and ds3231_get_alarm_s() always operate in 24-hour mode and translation
 * has to be done manually (you can call ds3231_24h_to_12h to perform
 * this calculation).
 */
struct time {
	uint8_t sec;            //!< Seconds [0;59].
	uint8_t min;            //!< Minutes [0;59].
	uint8_t hour;           //!< Hours [0;23].
	uint8_t mday;           //!< Date [0;31].
	uint8_t mon;            //!< Month [1;12].
	uint8_t year;           //!< Year [1900;2099].
	uint8_t wday;           //!< Day of the week [1;7].

	bool am;                //!< AM/PM (true/false) indicator.
	uint8_t twelveHour;     //!< Twelve hour time [0;11].
};

extern struct time _time;   //!< Time stored at the last update.

/**Gets the current time from the DS3231.
 *
 * @param[out]	time_	Time struct to which to copy the time.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_get_time(struct time* time_);
/**Gets the current time from the DS3231.
 *
 * @param[out]	hour	The byte to which to copy the current hour from DS3231.
 * @param[out]	min		The byte to which to copy the current minute from DS3231.
 * @param[out]	sec		The byte to which to copy the current second from DS3231.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_get_time_s(uint8_t* hour, uint8_t* min, uint8_t* sec);
/**Sets the time of the DS3231.
 *
 * @param[in]	time_	Time struct from which to copy the time.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_set_time(struct time* time_);
/**Sets the time of the DS3231.
 *
 * @param[in]	hour	The hour to set the DS3231 to.
 * @param[in]	min		The minute to set the DS3231 to.
 * @param[in]	sec		The second to set the DS3231 to.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_set_time_s(uint8_t hour, uint8_t min, uint8_t sec);

/**Gets the temperature from the DS3231.
 *
 * @param[out]	i		The integer part of the temperature.
 * @param[out]	f		The fraction part of the temperature (f/4).
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_get_temp_int(int8_t* i, uint8_t* f);
/**
 *
 * @param[in]	block
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_force_temp_conversion(uint8_t block);

/**Controls the 1 Hz square wave output.
 *
 * @param[in]	enable	The state to which to set the square wave generator.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_SQW_enable(bool enable);
/**Controls the 32 kHz square wave output.
 *
 * @param[in]	enable	The state to which to set the 32 kHz generator.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_osc32kHz_enable(bool enable);

/**Resets the alarm.
 *
 * @param[in]	alarm	Which alarm to reset (ALARM_1 or ALARM_2).
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_reset_alarm(uint8_t alarm);
/**Sets the alarm.
 *
 * @param[in]	time_	The time to which to set the alarm to.
 * @param[in]	alarm	The alarm to set.
 * @param[in]	mode	The resolution to which to set the alarm to.
 * @param[in]	intrpt	Whether or not the alarm should generate an interrupt.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_set_alarm(struct time* time_, uint8_t alarm, uint8_t mode, bool intrpt);
/**Sets the alarm.
 *
 * @param[in]	day		The week day/date to which to set the alarm to, depending on mode.
 * @param[in]	hour	The hour to which to set the alarm to.
 * @param[in]	min		The minute to which to set the alarm to.
 * @param[in]	sec		The second to which to set the alarm to.
 * @param[in]	alarm	The alarm to set.
 * @param[in]	mode	The resolution to which to set the alarm to.
 * @param[in]	intrpt	Whether or not the alarm should generate an interrupt.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_set_alarm_s(uint8_t day, uint8_t hour, uint8_t min, uint8_t sec, uint8_t alarm, uint8_t mode, bool intrpt);
/**Gets the alarm.
 *
 * @param[out]	time_	The time of the alarm.
 * @param[in]	alarm	The alarm to get.
 * @param[out]	mode	The resolution of the alarm.
 * @param[out]	intrpt	Whether or not this alarm will generate an interrupt.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_get_alarm(struct time* time_, uint8_t alarm, uint8_t* mode, bool* intrpt);
/**Gets the alarm.
 *
 * @param[out]	day		The week day/date of the alarm, depending on mode.
 * @param[out]	hour	The hour of the alarm.
 * @param[out]	min		The minute of the alarm.
 * @param[out]	sec		The second of the alarm.
 * @param[in]	alarm	The alarm to get.
 * @param[out]	mode	The resolution of the alarm.
 * @param[out]	intrpt	Whether or not this alarm will generate an interrupt.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_get_alarm_s(uint8_t* day, uint8_t* hour, uint8_t* min, uint8_t* sec, uint8_t alarm, uint8_t* mode, bool* intrpt);
/**Checks whether the alarm has been activated.
 *
 * @param[out]	active	Whether or not the alarm has been activated.
 * @param[in]	alarm	Which alarm to check.
 *
 * @return				Returns TRUE (1) if time was gotten successfully, otherwise FALSE (0).
 */
uint8_t ds3231_check_alarm(bool* active, uint8_t alarm);