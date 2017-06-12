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

/**@file twi.h
 * @author Valters Melnalksnis
 * @date 4 Jan 2017
 * @brief File containing definitions and prototypes for TWI
 *
 */
// Defines controlling timing limits
#define TWI_FAST_MODE                            //!< Must be defined for clock speeds [100;400] kHz.

#define SYS_CLK 4000.0                           //!< TWI clock speed in kHz.

#ifdef TWI_FAST_MODE                             // TWI FAST mode timing limits. SCL = 100-400kHz
	// >1.3us
	#define T2_TWI ((SYS_CLK * 1300) / 1000000) + 1
	// >0.6us
	#define T4_TWI ((SYS_CLK * 600) / 1000000) + 1
#else                                            // TWI STANDARD mode timing limits. SCL <= 100kHz
	// >4.7us
	#define T2_TWI ((SYS_CLK * 4700) / 1000000) + 1
	// >4.0us
	#define T4_TWI ((SYS_CLK * 4000) / 1000000) + 1
#endif

// Controlling code generation definitions
#define PARAM_VERIFICATION                       //!<
#define NOISE_TESTING                            //!<
#define SIGNAL_VERIFY                            //!<

// Bit and byte definitions
#define TWI_READ_BIT 0                           //!< Bit position for R/W bit in "address byte"
#define TWI_ADR_BITS 1                           //!< Bit position for LSB of the slave address bits in the initialization byte
#define TWI_NACK_BIT 0                           //!< The position for (N)ACK bit

#define TWI_NO_DATA            0x00              //!< Transmission buffer is empty
#define TWI_DATA_OUT_OF_BOUND  0x01              //!< Transmission buffer is outside SRAM space
#define TWI_UE_START_CON       0x02              //!< Unexpected Start Condition
#define TWI_UE_STOP_CON        0x03              //!< Unexpected Stop Condition
#define TWI_UE_DATA_COL        0x04              //!< Unexpected Data Collision (arbitration)
#define TWI_NO_ACK_ON_DATA     0x05              //!< The slave did not acknowledge all data
#define TWI_NO_ACK_ON_ADDRESS  0x06              //!< The slave did not acknowledge the address
#define TWI_MISSING_START_CON  0x07              //!< Generated Start Condition not detected on bus
#define TWI_MISSING_STOP_CON   0x08              //!< Generated Stop Condition not detected on bus

// Device dependent defines
#if defined(__AVR_AT90Mega169__) | defined(__AVR_ATmega169PA__) | \
	defined(__AVR_AT90Mega165__) | defined(__AVR_ATmega165__)   | \
	defined(__AVR_ATmega325__)   | defined(__AVR_ATmega3250__)  | \
	defined(__AVR_ATmega645__)   | defined(__AVR_ATmega6450__)  | \
	defined(__AVR_ATmega329__)   | defined(__AVR_ATmega3290__)  | \
	defined(__AVR_ATmega649__)   | defined(__AVR_ATmega6490__)  | \
	defined(__AVR_ATmega169P__)

	#define DDR_TWI     DDRE
	#define PORT_TWI    PORTE
	#define PIN_TWI     PINE
	#define PIN_TWI_SDA PINE5
	#define PIN_TWI_SCL PINE4
#endif

#if defined(__AVR_ATtiny25__) | defined(__AVR_ATtiny45__)   | \
	defined(__AVR_ATtiny85__) | defined(__AVR_AT90Tiny26__) | \
	defined(__AVR_ATtiny26__)

	#define DDR_TWI     DDRB
	#define PORT_TWI    PORTB
	#define PIN_TWI     PINE
	#define PIN_TWI_SDA PINB0
	#define PIN_TWI_SCL PINB2
#endif

#if defined(__AVR_AT90Tiny2313__) | defined(__AVR_ATtiny2313__)

	#define DDR_TWI     DDRB
	#define PORT_TWI    PORTB
	#define PIN_TWI     PINE
	#define PIN_TWI_SDA PINB5
	#define PIN_TWI_SCL PINB7
#endif

/**Sets the USI module in TWI mode, and the TWI bus in idle/released mode.
 *
 */
void TWI_master_initialize(void);
/**Sends or receives a byte array of defined length.
 *
 * @param[in,out] msg        Transmission buffer. First location must contain slave address and R/W (1/0) bit.
 * @param[in]     msgSize    Number of bytes in the transmission buffer.
 * @return                   Returns 1 if transmission was completed successfully, otherwise 0.
 */
uint8_t TWI_start_transceiver_with_data(uint8_t *msg, uint8_t msgSize);
/**Gets the error information about the last transmission
 *
 * @return                   Returns the error information about the last transmission.
 */
uint8_t TWI_get_state_info(void);