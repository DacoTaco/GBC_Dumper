/*
gb_pins - An AVR library to communicate with a GB/C/A cartridge. this is the pin setup
Copyright (C) 2018-2019  DacoTaco
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <inttypes.h>
#include <avr/interrupt.h>

//define all pins for a specific chip!
#ifdef GPIO_EXTENDER_MODE	
	
	#define GET_DATA(x) { mcp23008_ReadReg(DATA_CHIP_1, GPIO,&x);}
	#define SET_DATA(x) { mcp23008_WriteReg(DATA_CHIP_1, GPIO,x);}
	
	#define SET_ADDR1(x) { mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(x & 0xFF)); }
	#define SET_ADDR(x) { mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(x & 0xFF));mcp23008_WriteReg(ADDR_CHIP_2,GPIO,(uint8_t)(x >> 8) & 0xFF); }
	#define GET_ADDR1_DATA(x) {mcp23008_ReadReg(ADDR_CHIP_1, GPIO,&x);}
	#define GET_ADDR2_DATA(x) {mcp23008_ReadReg(ADDR_CHIP_2, GPIO,&x);}
	
	#define CTRL_DDR DDRC
	#define CTRL_PORT PORTC
	#define CTRL_PIN PINC
	
	//the MCP23008 addresses consist of 0b0100xxxy. xxx = the chip set address & y = the read/write bit.
	//but no worries, the mcp23008 library keeps that in check for us in case we fuck up
	#define BASE_ADDR_CHIPS 0b01000000
		
	#define ADDR_CHIP_1 (0b01000000)
	#define ADDR_CHIP_2 (0b01000010)
	#define DATA_CHIP_1 (0b01000100)
	
	
	#define RD PC3
	#define WD PC2
	#define CS1 PC4
	#define CS2 PC5
	#define BTN PC1
	
#elif defined(SHIFTING_MODE)

	#define DATA_PORT PORTB
	#define DATA_DDR DDRB
	#define DATA_PIN PINB
	
	#define GET_DATA(x) (x = DATA_PIN)
	#define SET_DATA(x) (DATA_PORT = byte)
	
	#define ADDR_CTRL_DDR DDRC
	#define ADDR_CTRL_PORT PORTC
	#define ADDR_CTRL_PIN PINC
	
	#define ADDR_CTRL_DATA PC2
	#define ADDR_CTRL_CLK PC3
	#define ADDR_CTRL_LATCH PC4
	
	#define CTRL_DDR DDRD
	#define CTRL_PORT PORTD
	#define CTRL_PIN PIND
	
	#define RD PD2
	//0b00100000;
	#define WD PD3
	//0b01000000;
	#define CS1 PD4
	//0b10000000;
	#define CS2 PD5
	//0b00000100;
	#define BTN PD6
	//0b00100000
	
#else
		
	#define DATA_PORT PORTA
	#define DATA_DDR DDRA
	#define DATA_PIN PINA
	
	#define ADDR_DDR1 DDRB
	#define ADDR_DDR2 DDRC
	#define ADDR_PORT1 PORTB
	#define ADDR_PORT2 PORTC
	#define ADDR_PIN1 PINB
	#define ADDR_PIN2 PINC
	
	//add nops as we were reading faster then a GB cart could output @ 8Mhz
	#define GET_DATA(x) {asm("nop");asm("nop");x = DATA_PIN;}
	#define SET_DATA(x) (DATA_PORT = x)
	
	#define SET_ADDR(x) { ADDR_PORT1 = x >> 8; ADDR_PORT2 = (uint8_t)(x & 0xFF); }
	#define GET_ADDR1_DATA(x) {asm("nop");x = ADDR_PIN1;}
	#define GET_ADDR2_DATA(x) {asm("nop");x = ADDR_PIN2;}

	
	#define CTRL_DDR DDRD
	#define CTRL_PORT PORTD
	#define CTRL_PIN PIND
	#define RD PD4
	//0b00000100;
	#define WD PD3
	//0b00001000;
	#define CS1 PD5
	//0b00010000;
	#define CS2 PD6
	//0b00100000;
	#define BTN PD7
	//0b01000000
	
#endif

#define HIGH 1
#define LOW 0

#define INPUT 0
#define OUTPUT 1

#define SetPin(x,y) (x |= (1<<y))
#define ClearPin(x,y) (x &= ~(1<<y))
int8_t CheckControlPin(uint8_t Pin);
void SetControlPin(uint8_t Pin,uint8_t state);
void SetDataPinsAsOutput(void);
void SetDataPinsAsInput(void);
void SetAddressPinsAsOutput(void);
void SetAddressPinsAsInput(void);
