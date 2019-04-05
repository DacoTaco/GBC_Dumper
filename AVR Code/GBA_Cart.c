/*
GBA_Cart - An AVR library to communicate with a GBA cartridge. accessing ROM & RAM
Copyright (C) 2016-2019  DacoTaco
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
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "gb_error.h"
#include "GBA_Cart.h"
#include "GB_Cart.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif

inline void SetGBADataAsOutput(void)
{
	//disable pull ups
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0x00);
	
	//set output
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0x00);
	
}
inline void SetGBADataAsInput(void)
{
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0xFF);
	
	//enable pull up
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0xFF);
}
void Setup_GBA_Pins(void)
{
#ifdef GPIO_EXTENDER_MODE
	//setup the mcp23008, as its the source of everything xD
	mcp23008_init(ADDR_CHIP_1);
	mcp23008_init(ADDR_CHIP_2);
	mcp23008_init(DATA_CHIP_1);

	SetGBADataAsOutput();	
#endif	

	//setup data pins as input
	SetDataPinsAsInput(); 
	
	//setup D pins as well for the other, as output
	CTRL_DDR |= ( (0 << BTN) | (1 << RD) | (1 << WD) | (1 << CS1) | (1 << CS2) );//0b01111100;
	//FUCK TRISTATE BULLSHIT xD set the mode of the pins correctly!
	CTRL_PORT |= ((1 << BTN) | (1 << RD) | (1 << WD) | (1 << CS1) | (1 << CS2) ); //0b01111100;
	
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	SetPin(CTRL_PORT,CS2);
}
inline void SetGBAAddress(uint32_t address)
{	
#ifdef GPIO_EXTENDER_MODE
	SetGBADataAsOutput();
	SetDataPinsAsOutput();
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,(uint8_t)(address & 0xFF));
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(address >> 8) & 0xFF);
	mcp23008_WriteReg(DATA_CHIP_1,GPIO,(uint8_t)(address >> 16) & 0xFF);
#endif
}
//The GBA supports something as a increment read.
//basically as long as CS1 is kept low, the next RD strobe will just reveal the next 2 bytes.
//so if you set the address, and read again, you'll get addr+1, next time addr+2 etc etc
//it'll only latch the lower 16 bits of the address though.
//this still saves us quiet a few cycles on setting everything.
inline uint16_t _ReadGBAIncrementedBytes(int8_t SetAddress,uint32_t address)
{
	uint8_t d1 = 0;
	uint8_t d2 = 0;
		
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,WD);
	
	if(SetAddress)
	{
		//do the whole shabang
		SetPin(CTRL_PORT,CS1);
		SetPin(CTRL_PORT,CS2);
		SetGBAAddress(address);
		ClearPin(CTRL_PORT,CS1);
	}
	
	ClearPin(CTRL_PORT,RD);
	
	SetGBADataAsInput();
	
#ifdef GPIO_EXTENDER_MODE			
	//read data
	mcp23008_ReadReg(ADDR_CHIP_1, GPIO,&d1);
	mcp23008_ReadReg(ADDR_CHIP_2, GPIO,&d2);	
#endif
	
	SetPin(CTRL_PORT,RD);
	return (uint16_t)d1 << 8 | d2;
}
inline uint16_t ReadGBABytes(uint32_t address)
{
	uint8_t d1 = 0;
	uint8_t d2 = 0;

	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,CS1);

	SetGBAAddress(address);
	
	//latch the address
	ClearPin(CTRL_PORT,CS1);
	
	//set RD low so data pins are set
	ClearPin(CTRL_PORT,RD);

#ifdef GPIO_EXTENDER_MODE	
	//set as input so we can read the pins
	SetGBADataAsInput();
#endif
	
#ifdef GPIO_EXTENDER_MODE			
	//read data
	mcp23008_ReadReg(ADDR_CHIP_1, GPIO,&d1);
	mcp23008_ReadReg(ADDR_CHIP_2, GPIO,&d2);	
#endif
	
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	
	return (uint16_t)d1 << 8 | d2;
}
inline uint8_t ReadGBARamByte(uint16_t address)
{
	SetPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,CS1);
	SetDataPinsAsInput();
	
	//set address
	SetGBADataAsOutput();
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,(uint8_t)(address & 0xFF));
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(address >> 8) & 0xFF);
	
	//latch the address
	ClearPin(CTRL_PORT,CS2);
	asm("nop");
	asm("nop");
	
	//set RD low so data pins are set
	ClearPin(CTRL_PORT,RD);
	
	//read data
	uint8_t data = 0;
	GET_DATA(data);
	
	SetPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,RD);
	
	return data;
}
uint32_t GetGBARomSize(void)
{
	uint32_t i = 0;
	uint16_t start[0x0F];
	uint8_t endFound = 0;
	
	//retrieve first few bytes of header to use to compare later
	for(uint8_t x = 0;x < 0x0F;x++)
	{
		start[x] = ReadGBABytes(x);
	}
	
	//basically we read all addresses untill we notice the rom starts mirroring. thats our size
	for(i = 0x00100000UL; i <= 0x01000000UL; i<<=1)
	{	
		for(uint8_t x = 0;x < 0x0F;x++)
		{
			if(ReadGBABytes(i+x) != start[x])
			{
				endFound = 0;
				break;
			}
			endFound = 1;
		}		
		if(endFound)
			break;	
	}
	return i;
}

int8_t GetGBAInfo(char* name)
{
	GBA_Header info;
	uint16_t* ptr = (uint16_t*)&info;
	uint8_t FF_Cnt = 0;
	//normal size is 0x9C. we only check 0x08 to speed things up and save space
	uint8_t Logo[0x08] = {
								0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21/*, 0x3D, 0x84, 0x82, 0x0A, 
		0x84, 0xE4, 0x09, 0xAD, 0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21, 0xA3, 0x52, 0xBE, 0x19,
		0x93, 0x09, 0xCE, 0x20, 0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC, 0x58, 0xC7, 0xE8, 0x33, 
		0x82, 0xE3, 0xCE, 0xBF, 0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1, 0x94, 0x56, 0x8A, 0xC0,
		0x13, 0x72, 0xA7, 0xFC, 0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61, 0x58, 0x97, 0xA3, 0x27,
		0xFC, 0x03, 0x98, 0x76, 0x23, 0x1D, 0xC7, 0x61, 0x03, 0x04, 0xAE, 0x56, 0xBF, 0x38, 0x84, 0x20,
		0x40, 0xA7, 0x0E, 0xFD, 0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1, 0x97, 0xFB, 0xC0, 0x85,
		0x60, 0xD6, 0x80, 0x25, 0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2, 0xF9, 0xA2, 0x34, 0xFF,
		0xBB, 0x3E, 0x03, 0x44, 0x78, 0x20, 0x90, 0xCB, 0x88, 0x11, 0x3A, 0x94, 0x65, 0xC0, 0x7C, 0x63,
		0x87, 0xF0, 0x3C, 0xAF, 0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72, 0x21, 0xD4, 0xF8, 0x07*/
	};
	
	//Read header
	for(uint8_t i = 0x00;i < 0xC0;i += 2 )
	{
		*ptr = ReadGBABytes(i/2);
	
		if(*ptr == 0xFFFF)
			FF_Cnt++;		
		if(FF_Cnt >= 3)
			return ERR_FAULT_CART;
		
		ptr++;
	}
	
	//compare logo. we only compare 8 bytes because we are lazy and saving space
	for(uint8_t i = 0x00;i < 0x08;i++)
	{
		if(Logo[i] != info.Logo[i])
			return ERR_LOGO_CHECK;
	}
	
	//compare fixed value
	if(info.FixedValue != 0x96)
		return ERR_FAULT_CART;
	
	//eh, it'll be ok. copy over the name...
	memset(name,0,13);
	ptr = (uint16_t*)name;
	uint16_t* ptr_name = (uint16_t*)info.Name;
	for(uint8_t i = 0x00;i < 0x0C;i += 2 )
	{
		*ptr = *ptr_name;
		ptr++;
		ptr_name++;
	}

	return 1;
}