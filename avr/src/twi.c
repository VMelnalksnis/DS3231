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

/**@file twi.c
 *
 */

#include <avr/io.h>
#include <compat/twi.h>
#include <stdbool.h>
#include <util/delay.h>

#include "twi.h"

uint8_t TWI_master_stop(void);
uint8_t TWI_master_transfer(uint8_t temp);

/**
 *
 *
 */
union TWI_state
{
	uint8_t errorState;

	struct
	{
		uint8_t addressMode : 1;
		uint8_t masterWrite : 1;
		uint8_t unused : 6;
	};
} TWI_state;

uint8_t TWI_get_state_info(void)
{
	return TWI_state.errorState;
}

void TWI_master_initialize(void)
{
	PORT_TWI |= (1 << PIN_TWI_SDA);              // Enable pullup on SDA, to set high as released state
	PORT_TWI |= (1 << PIN_TWI_SCL);              // Enable pullup on SCL, to set high as released state

	DDR_TWI |= (1 << PIN_TWI_SDA);               // Enable SDA as output
	DDR_TWI |= (1 << PIN_TWI_SCL);               // Enable SCL as output

	USIDR =	0xFF;                                // Preload data register with "released level" data
	USICR = (0 << USISIE) | (0 << USIOIE) |      // Disable interrupts
	        (1 << USIWM1) | (1 << USIWM0) |      // Set USI in two-wire mode
	        (1 << USICS1) | (0 << USICS0) |      // Set shift register clock source as external, positive edge
	        (1 << USICLK) |                      // Set 4-bit counter clock source as software clock strobe
	        (0 << USITC);                        // Do nothing with Toggle Clock
	USISR = (1 << USISIF) | (1 << USIOIF) |      // Clear Start Condition and counter overflow flags
	        (1 << USIPF)  | (1 << USIDC)  |      // Clear Stop Condition and data output collision flags
	        (0 << USICNT0);                      // Reset counter
}

uint8_t TWI_start_transceiver_with_data(uint8_t *msg, uint8_t msgSize)
{
	uint8_t tempUSISR_8bit = (1 << USISIF) |     // Prepare register value to:
	                         (1 << USIOIF) |     // Clear flags, and set USI to
	                         (1 << USIPF)  |     // shift 8 bits i.e. count 16 clock edges
	                         (1 << USIDC)  |
	                         (0x0 << USICNT0);
	uint8_t tempUSISR_1bit = (1 << USISIF) |     // Prepare register value to:
	                         (1 << USIOIF) |     // Clear flags, and set USI to
	                         (1 << USIPF)  |     // shift 1 bit i.e. count 2 clock edges
	                         (1 << USIDC)  |
	                         (0xE << USICNT0);

	TWI_state.errorState = 0;
	TWI_state.addressMode = true;

#ifdef PARAM_VERIFICATION
	if (msg > (uint8_t*)RAMEND)                  // Test if address is outside SRAM space
	{
		TWI_state.errorState = TWI_DATA_OUT_OF_BOUND;
		return (false);
	}
	if (msgSize <= 1)                            // Test if the transmission buffer is empty
	{
		TWI_state.errorState = TWI_NO_DATA;
		return (false);
	}
#endif

#ifdef NOISE_TESTING
	if(USISR & (1 << USISIF))
	{
		TWI_state.errorState = TWI_UE_START_CON;
		return (false);
	}
	if(USISR & (1 << USIPF))
	{
		TWI_state.errorState = TWI_UE_STOP_CON;
		return (false);
	}
	if(USISR & (1 << USIDC))
	{
		TWI_state.errorState = TWI_UE_DATA_COL;
		return (false);
	}
#endif

	if(!(*msg & (1 << TWI_READ_BIT)))            // The LSB in the address byte determines if this is a
	{                                            // masterRead or masterWrite operation
		TWI_state.masterWrite = true;
	}

	                                             // Release SCL to ensure that (repeated) Start can be performed
	PORT_TWI |= (1 << PIN_TWI_SCL);
	while (!(PIN_TWI & (1 << PIN_TWI_SCL)));
#ifdef TWI_FAST_MODE
	_delay_us(T4_TWI / 4);
#else
	_delay_us(T2_TWI / 4);
#endif

	                                             // Send a Start Condition on the TWI bus
	PORT_TWI &= ~(1 << PIN_TWI_SDA);             // Force SDA LOW
	_delay_us(T4_TWI / 4);
	PORT_TWI &= ~(1 << PIN_TWI_SCL);             // Pull SCL LOW
	PORT_TWI |= (1 << PIN_TWI_SDA);              // Release SDA

#ifdef SIGNAL_VERIFY
	if(!(USISR & (1 << USISIF)))
	{
		TWI_state.errorState = TWI_MISSING_START_CON;
		return (true);
	}
#endif

	do                                           // Write address and read/write data
	{
		                                         // masterWrite cycle or initial address transmission
		if(TWI_state.addressMode || TWI_state.masterWrite)
		{
			                                     // Write a byte
			PORT_TWI &= ~(1 << PIN_TWI_SCL);     // Pull SCL LOW
			USIDR = *(msg++);                    // Setup data
			TWI_master_transfer(tempUSISR_8bit); // Send 8 bits on the bus
			                                     // Clock and verify (N)ACK from slave
			DDR_TWI &= ~(1 << PIN_TWI_SDA);      // Enable SDA as input
			if(TWI_master_transfer(tempUSISR_1bit) & (1 << TWI_NACK_BIT))
			{
				if (TWI_state.addressMode)
				{
					TWI_state.errorState = TWI_NO_ACK_ON_ADDRESS;
				}
				else
				{
					TWI_state.errorState = TWI_NO_ACK_ON_DATA;
				}

				return (false);
			}
			TWI_state.addressMode = false;       // Perform address transmission only once
		}
		else                                     // masterRead cycle
		{
			DDR_TWI &= ~(1 << PIN_TWI_SDA);	     // Enable SDA as input
			*(msg++) = TWI_master_transfer(tempUSISR_8bit);
			                                     // Prepare to generate (N)ACK
			if (msgSize == 1)                    // If transmission of last byte was performed
			{
				USIDR = 0xFF;                    // Load NACK to confirm End of Transmission
			}
			else
			{
				USIDR = 0x00;                    // Load ACK; set data register bit 7 (output for SDA) low
			}
			TWI_master_transfer(tempUSISR_1bit); // Generate (N)ACK
		}
	} while (--msgSize);                         // Until all data sent/received

	TWI_master_stop();                           // Send a Stop Condition on the TWI bus

	return (true);                               // Transmission completed successfully
}

uint8_t TWI_master_transfer(uint8_t temp)
{
	USISR = temp;                                // Set USISR according to temp
	// Prepare clocking
	temp  = (0 << USISIE) | (0 << USIOIE) |      // Disable interrupts
	        (1 << USIWM1) | (1 << USIWM0) |      // Set USI in two-wire mode
	        (1 << USICS1) | (0 << USICS0) |      // Set shift register clock source as external, positive edge
	        (1 << USICLK) |                      // Set 4-bit counter clock source as software clock strobe
	        (0 << USITC);                        // Do nothing with Toggle Clock

	do
	{
		_delay_us(T2_TWI / 4);
		USICR = temp;                            // Generate positive SCL edge
		while (!(PIN_TWI & (1 << PIN_TWI_SCL))); // Wait for SCL to go HIGH
		_delay_us(T4_TWI / 4);
		USICR = temp;                            // Generate negative SCL edge
	} while (!(USISR & (1 << USIOIF)));          // Check for transfer complete

	_delay_us(T2_TWI / 4);
	temp = USIDR;                                // Read out data
	USIDR = 0xFF;                                // Release SDA
	DDR_TWI |= (1 << PIN_TWI_SDA);               // Enable SDA as output

	return temp;
}

uint8_t TWI_master_stop(void)
{
	PORT_TWI &= ~(1 << PIN_TWI_SDA);             // Pull SDA LOW
	PORT_TWI |= (1 << PIN_TWI_SCL);              // Release SCL
	while(!(PIN_TWI & (1 << PIN_TWI_SCL)));      // Wait for SCL to go HIGH
	_delay_us(T4_TWI / 4);
	PORT_TWI |= (1 << PIN_TWI_SDA);              // Release SDA
	_delay_us(T2_TWI / 4);

#ifdef SIGNAL_VERIFY
	if(!(USISR & (1 << USIPF)))
	{
		TWI_state.errorState = TWI_MISSING_STOP_CON;
		return (false);
	}
#endif

	return (true);
}