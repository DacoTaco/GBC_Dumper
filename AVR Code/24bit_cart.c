/*
24bit_cart - An AVR library to communicate with a GBA cartridge. accessing ROM
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
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "gb_error.h"
#include "gb_pins.h"
#include "8bit_cart.h"
#include "24bit_cart.h"
#include "serial.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif

void Setup_Pins_24bitMode(void)
{
#ifdef GPIO_EXTENDER_MODE
	//setup the mcp23008, as its the source of everything xD
	mcp23008_init(ADDR_CHIP_1);
	mcp23008_init(ADDR_CHIP_2);
	mcp23008_init(DATA_CHIP_1);	
#endif	
	SetAddressPinsAsOutput();
	
	//setup data pins as input
	SetDataPinsAsInput(); 
	
	//setup D pins as well for the other, as output
	CTRL_DDR |= ( (0 << BTN) | (1 << RD) | (1 << WD) | (1 << CS1) | (1 << CS2) );
	//FUCK TRISTATE BULLSHIT xD set the mode of the pins correctly!
	CTRL_PORT |= ((1 << BTN) | (1 << RD) | (1 << WD) | (1 << CS1) | (1 << CS2) );
	
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	SetPin(CTRL_PORT,CS2);
}
inline void Set24BitAddress(uint32_t address)
{	
	SetAddressPinsAsOutput();
	SetDataPinsAsOutput();
#ifdef GPIO_EXTENDER_MODE	
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(address & 0xFF));
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,(uint8_t)(address >> 8) & 0xFF);
	mcp23008_WriteReg(DATA_CHIP_1,GPIO,(uint8_t)(address >> 16) & 0xFF);
#else
	ADDR_PORT1 = address >> 8;
	ADDR_PORT2 = (uint8_t)(address & 0xFF);
	SET_DATA((uint8_t)(address >> 16) & 0xFF);
#endif
}
//The GBA supports something as a increment read.
//basically as long as CS1 is kept low, the next RD strobe will just reveal the next 2 bytes.
//so if you set the address, and read again, you'll get addr+1, next time addr+2 etc etc
//it'll only latch the lower 16 bits of the address though.
//this still saves us quiet a few cycles on setting everything.
inline uint16_t Read24BitIncrementedBytes(int8_t LatchAddress,uint32_t address)
{
	uint8_t d1 = 0;
	uint8_t d2 = 0;
		
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,WD);
	
	if(LatchAddress)
	{
		//do the whole shabang
		SetPin(CTRL_PORT,CS1);
		SetPin(CTRL_PORT,CS2);
		//set address
		Set24BitAddress(address);
		//latch address
		ClearPin(CTRL_PORT,CS1);
		//set pins as input
		SetAddressPinsAsInput();
	}
	
	ClearPin(CTRL_PORT,RD);		
	asm("nop");
	asm("nop");
	
#ifdef GPIO_EXTENDER_MODE			
	//read data
	mcp23008_ReadReg(ADDR_CHIP_2, GPIO,&d1);
	mcp23008_ReadReg(ADDR_CHIP_1, GPIO,&d2);	
#else
	GET_ADDR1_DATA(d2);
	GET_ADDR2_DATA(d1);
#endif
	
	SetPin(CTRL_PORT,RD);
	return (uint16_t)d1 << 8 | d2;
}
inline uint16_t Read24BitBytes(uint32_t address)
{
	uint16_t data = Read24BitIncrementedBytes(1,address);
	SetPin(CTRL_PORT,CS1);
	
	return data;
}
uint32_t GetGBARomSize(void)
{
	uint32_t i = 0;
	uint8_t endFound = 0;
	
	//basically we read all addresses untill we notice the rom starts mirroring. 
	//from testing a few carts i noticed it started to mirror, instead of throwing 00's (open bus)?
	//thats our size
	for(i = 0x00100000UL; i <= 0x01000000UL; i<<=1)
	{	
		for(uint8_t x = 0;x < 0x0F;x++)
		{
			if(Read24BitBytes(i+x) != Read24BitBytes(x))
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

int8_t GetGBAInfo(char* name, uint8_t* cartFlag)
{
	if(name == NULL || cartFlag == NULL)
		return ERR_NO_INFO;
	
	GBA_Header info;
	uint16_t* ptr = (uint16_t*)&info;
	*cartFlag = GBA_SAVE_NONE;
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
		*ptr = Read24BitIncrementedBytes(i==0,i/2);
	
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
	
	//cart is detected OK lets set all data
	
	//check if cart is SRAM or flash
	*cartFlag = GBA_CheckForSave();
	
	
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
uint32_t GetGBARamSize(uint8_t RamType)
{
	if(RamType != GBA_SAVE_FLASH && RamType != GBA_SAVE_SRAM && RamType != GBA_SAVE_SRAM_FLASH)
		return 0;
	
	//size calculation for flash & Sram
	//Sram only has one size (on paper...). if we read past 0x8000 we would see that it is a mirror of 0x0000	
	uint8_t duplicates = 0;
	for(uint16_t i = 0;i < 0x400;i++)
	{
		if(ReadGBARamByte(i) == ReadGBARamByte(i+0x8000))
		{
			duplicates++;
		}
	}
	
	//32KB ( 256Kbit )
	if(duplicates == 0x400)
		return 0x8000;
	
	//64KB SRAM, max Sram size ( 512Kbit )
	if(GBA_CheckForSramOrFlash() == GBA_SAVE_SRAM)
		return 0x10000;
	
	//check if we are dealing with a 64KB or 128KB flash ( 512Kbit or 1Mbit)
	return 0;
}

uint8_t GBA_CheckForSave(void)
{
	//set to 8bit mode
	Setup_Pins_8bitMode();
	//read 32x64 bytes and check for bogus
	uint16_t addr = 0x00;
	uint8_t zeroes = 0;
	for (uint8_t x = 0; x < 32; x++) 
	{		
		zeroes = 0;
		for(uint8_t i = 0;i < 64;i++)
		{
			uint8_t data = ReadGBARamByte(addr+i);
			if(data == 0)
			{				
				zeroes++;	
			}	
		}
		if(x == 0 && zeroes >= 63)
		{
			Setup_Pins_24bitMode();
			return GBA_SAVE_NONE;
		}
		addr += 0x400;
	}

	Setup_Pins_24bitMode();
	return GBA_SAVE_SRAM_FLASH;
}
uint8_t GBA_CheckForSramOrFlash(void)
{
	return GBA_SAVE_SRAM;
}