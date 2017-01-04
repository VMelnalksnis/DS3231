# TWI/I2C real-time clock library

A library for the DS3231 real-time clock for megaAVR and tinyAVR devices (supported devices can be seen and/or added in twi.h by defining appropriate pins and registers). 

Available features:
* Set and get time
* Set, get and check alarms
* Control the 1 Hz and 32 kHz square wave oscillator outputs. When in use, a pull-up resistor is required on the output pin and the 1 Hz output replaces alarm interrupts

Future features:
* Read temperature/force temperature conversion
* Ability to operate in 12-hour mode. Currently 12-hour mode is implemented only in software
