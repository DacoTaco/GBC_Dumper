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

	SET_ADDR(address);
	SET_DATA((uint8_t)(address >> 16) & 0xFF);
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
		
		//now that the cart has latched the address, we can set the address pins as 0, to force 0x0000 when open bus
		SET_ADDR(0x0000);
		
		//set pins as input
		SetAddressPinsAsInput();
	}
	
	ClearPin(CTRL_PORT,RD);
	asm("nop");
	asm("nop");
	
	GET_ADDR1_DATA(d2);
	GET_ADDR2_DATA(d1);
	
	SetPin(CTRL_PORT,RD);
	return (uint16_t)d1 << 8 | d2;
}
inline uint16_t Read24BitBytes(uint32_t address)
{
	uint16_t data = Read24BitIncrementedBytes(1,address);
	SetPin(CTRL_PORT,CS1);
	
	return data;
}
void SetEepromRamAddress(uint16_t address, int8_t eeprom_type)
{
	uint16_t mask = 0b00000000010000000;
	int8_t size = 8;
	
	if(eeprom_type)
	{
		mask = 0b1000000000000000;
		size = 16;
	}
	
	//setup pins
	SetAddressPinsAsOutput();
	ClearPin(CTRL_PORT,CS1);
	
	for (int8_t x = 0;x < size; x++)
	{
		char byte = 0x00;
		if((address << x) & mask)
			byte = 0x01;
		
		SET_ADDR1(byte);	
		ClearPin(CTRL_PORT,WD); //clk
		//asm ("nop");
		SetPin(CTRL_PORT,WD); //clk
	}
	
	//stop command can be send. only for reading since writing it done in 1 huge chunk
	//the command for reading is 11 & writing is 10, so if we read the 2nd command byte we know if its reading or writing
	if (address & (mask >> 1)) 
	{  
		SET_ADDR1(0x00);
		ClearPin(CTRL_PORT,WD);
		//asm ("nop");	
		SetPin(CTRL_PORT,WD);
		//asm ("nop");
		SetPin(CTRL_PORT,CS1);
	}
}
//NOTE : reading EEPROM needs to be done in chucks of 8 bytes/64bits.
//hence the buffer pointer
void ReadEepromRamByte(uint16_t address, int8_t eeprom_type, uint8_t* buffer)
{
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,CS1);
	
	//clear the operation bits first
	//address &= ~GBA_EEPROM_READ;
	
	//append read command
	address = address | ( eeprom_type == EEPROM_TYPE_64KBIT? GBA_EEPROM_READ_64KBIT : GBA_EEPROM_READ_4KBIT);
	
	//pass Address to cartridge via the address bus + Read bits
	SetEepromRamAddress(address,eeprom_type);
	SetAddressPinsAsInput();
	ClearPin(CTRL_PORT,CS1);
	
	// Ignore first 4 bits
	for (int8_t x = 0; x < 4; x++) 
	{
		ClearPin(CTRL_PORT,RD);
		//asm ("nop");
		SetPin(CTRL_PORT,RD);
		//asm ("nop");
	}
	
	// Read out 64 bits
	for (uint8_t c = 0; c < 8; c++) 
	{
		uint8_t data = 0;
		uint8_t y = 0;	
		for (int8_t x = 7; x >= 0; x--) 
		{
			ClearPin(CTRL_PORT,RD);
			//asm ("nop");
			SetPin(CTRL_PORT,RD);
			
			GET_ADDR1_DATA(y);		
			data |= ((y & 0x01)<<x);
		}
		buffer[c] = data;
	}
	
	SetPin(CTRL_PORT,CS1);
	return;
}
uint32_t GetGBARomSize(void)
{
	uint32_t i = 0;
	uint8_t endFound = 0;
	uint16_t noData;
	
	//basically we read all addresses untill we notice the rom starts mirroring OR we get like... 0x1000 bytes of 0x0000 (open bus)
	//from testing a few carts i noticed it started to mirror (metroid fusion, sword of mana)
	//yet others went open bus (super mario advance 4) and read 0x00...
	//or does both (metroid fusion(EU))
	//thats our size
	for(i = 0x00100000UL; i < 0x01000000UL; i<<=1)
	{	
		noData = 0;
		for(uint16_t x = 0;x < 0x500;x++)
		{
			uint16_t data = Read24BitBytes(i+x);
			if(data == 0x0000)
			{
				noData++;
			}
			else if(data != Read24BitBytes(x))
			{
				endFound = 0;
				break;
			}
			else
				endFound = 1;
		}		
		if(endFound == 1 || noData == 0x500)
			break;	
	}
	
	//since GBA is 16bit per address, we need to multiply the size with 2
	if(i<= 0x01000000UL)
		i *= 2;
	
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
	
		if(*ptr == 0x0000)
			FF_Cnt++;		
		if(FF_Cnt >= 0x30)
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
uint32_t GetGBARamSize(uint8_t* RamType)
{
	if(RamType == NULL || (*RamType != GBA_SAVE_FLASH && *RamType != GBA_SAVE_SRAM && *RamType != GBA_SAVE_SRAM_FLASH))
		return 0;
	
	//size calculation for flash & Sram
	//Sram only has one size (on paper...). if we read past 0x8000 we would see that it is a mirror of 0x0000	
	Setup_Pins_8bitMode();
	uint16_t duplicates = 0;
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
	*RamType = GBA_CheckForSramOrFlash();
	if(*RamType == GBA_SAVE_SRAM)
		return 0x10000;
	else if (*RamType == GBA_SAVE_NONE)
		return 0;
	
	//check if we are dealing with a 64KB or 128KB flash ( 512Kbit or 1Mbit)
	//this can be done by comparing bank 0 reads to bank 1 and check for mirroring
	uint8_t bank0[64] = {0};
	duplicates = 0;
	
	for(uint8_t i = 0; i < 32 ; i++)
	{
		SwitchFlashRAMBank(0);
		//read from bank 0
		for(uint8_t x = 0; x < 64 ; x++)
		{
			bank0[x] = ReadGBARamByte((i*0x400)+x);
		}
		
		SwitchFlashRAMBank(1);
		//compare to bank 1
		for(uint8_t x = 0; x < 64 ; x++)
		{
			if ( bank0[x] == ReadGBARamByte((i*0x400)+x))
				duplicates++;
		}
	}
	
	//64KB
	if(duplicates >= 0x800)
		return 0x10000;
	//128KB
	return 0x20000;
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
	//ok so, how this works freaks me out but there doesn't seem to be a more reliable way
	//we basically write a test byte to address 0 of the save. if it returns as the test byte, we have sram and we restore what was on address 0.
	//if it doesn't return the test byte , we have flash
	//this because flash is read as normal, but writing requires a whole process...
	uint8_t testByte = 0xDA;
	uint8_t checkByte = 0x00;
	
	checkByte = ReadGBARamByte(0);
	if(checkByte == testByte)
		testByte = 0xED;
	
	//here we go... D:
	WriteGBARamByte(0,testByte);
	if(ReadGBARamByte(0) == testByte)
	{
		//we have SRAM!!
		//restore byte and return
		WriteGBARamByte(0,checkByte);
		return GBA_SAVE_SRAM;		
	}
	
	//TODO : read Flash manufactoring ID and verify it
	
	return GBA_SAVE_FLASH;
}