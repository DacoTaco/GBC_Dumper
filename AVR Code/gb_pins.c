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
#include "gb_pins.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif


//pin functions
//--------------------------------
inline void SetControlPin(uint8_t Pin,uint8_t state)
{
	if(state > 0)
	{
		SetPin(CTRL_PORT,Pin); // pin goes high
	}	
	else
	{
		ClearPin(CTRL_PORT,Pin); // Pin goes low
	}	
}
int8_t CheckControlPin(uint8_t Pin)
{
	if((CTRL_PIN & (1<< Pin)) == 0)
		return LOW;
	return HIGH;
}

inline void SetDataPinsAsInput(void)
{
	//set as input;
#ifdef GPIO_EXTENDER_MODE	
	mcp23008_WriteReg(DATA_CHIP_1,IODIR,0xFF);
	
	//enable pull up
	mcp23008_WriteReg(DATA_CHIP_1,GPPU,0xFF);
#else
	DATA_DDR &= ~(0xFF); //0b00000000;
	//enable pull up resistors
	DATA_PORT = 0xFF;
#endif
	return;
}
inline void SetDataPinsAsOutput(void)
{
	//set as output
#ifdef GPIO_EXTENDER_MODE
	//disable pull ups
	mcp23008_WriteReg(DATA_CHIP_1,GPPU,0x00);
	
	//set output
	mcp23008_WriteReg(DATA_CHIP_1,IODIR,0x00);	
#else
	DATA_DDR |= (0xFF);//0b11111111;
	//set output as 0x00
	DATA_PORT = 0x00;
#endif
}

inline void SetAddressPinsAsOutput(void)
{
#ifdef GPIO_EXTENDER_MODE
	//disable pull ups
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0x00);
	
	//set output
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0x00);
#else
	ADDR_DDR1 = 0xFF;
	ADDR_DDR2 = 0xFF;
	
	ADDR_PORT1 = 0x00;
	ADDR_PORT2 = 0x00;
#endif
	
}
inline void SetAddressPinsAsInput(void)
{
#ifdef GPIO_EXTENDER_MODE
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0xFF);
	
	//enable pull up
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0xFF);
#else
	ADDR_DDR1 &= ~(0xFF);
	ADDR_DDR2 &= ~(0xFF);
	
	ADDR_PORT1 = 0xFF;
	ADDR_PORT2 = 0xFF;
#endif
}

