/*
8bit_cart - An AVR library to communicate with a GB/C cartridge. accessing ROM & RAM
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
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "gb_error.h"
#include "8bit_cart.h"
#include "gb_pins.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif

void Setup_Pins_8bitMode(void)
{
#ifdef GPIO_EXTENDER_MODE
	//setup the mcp23008, as its the source of everything xD
	mcp23008_init(ADDR_CHIP_1);
	mcp23008_init(ADDR_CHIP_2);
	mcp23008_init(DATA_CHIP_1);
	
#elif defined(SHIFTING_MODE)
	//set the pins as output, init-ing the pins for the shift register
	ADDR_CTRL_DDR |= ( (1 << ADDR_CTRL_DATA) | (1 << ADDR_CTRL_CLK) | (1 << ADDR_CTRL_LATCH) );	
#endif	

	//set address pins as output
	SetAddressPinsAsOutput();
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

inline void Set8BitAddress(uint16_t address)
{	
#ifdef GPIO_EXTENDER_MODE	
	//write lower address
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(address & 0xFF));
	
	//write upper address
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,address >> 8);

#elif defined(SHIFTING_MODE)

	//write the 16 bits to the shift register
	ClearPin(ADDR_CTRL_PORT,ADDR_CTRL_LATCH);
	for(uint8_t i=0;i<16;i++)
	{
		if(address & 0b1000000000000000)
		{
			//set the data bit as high
			SetPin(ADDR_CTRL_PORT,ADDR_CTRL_DATA);
		}
		else
		{
			//set the data bit as low
			ClearPin(ADDR_CTRL_PORT,ADDR_CTRL_DATA);
		}
		
		//pulse the shifting register to set the bit
		SetPin(ADDR_CTRL_PORT,ADDR_CTRL_CLK);
		_delay_us(1);
		ClearPin(ADDR_CTRL_PORT,ADDR_CTRL_CLK);
		
		//shift with 1 bit, so we can take on the next bit
		address = address << 1;
	}	
	//all bits transfered. time to let the shifting register latch the data and set the pins accordingly
	SetPin(ADDR_CTRL_PORT,ADDR_CTRL_LATCH);
	
#else //#elif defined(NORMAL_MODE)
	uint8_t adr2 = address >> 8;
	uint8_t adr1 = (uint8_t)(address & 0xFF);
	
	ADDR_PORT1 = adr1;
	ADDR_PORT2 = adr2;
#endif
}
inline uint8_t _Read8BitByte(uint8_t CS_Pin, uint16_t address)
{
	uint8_t data = 0;
		
	//pass Address to cartridge via the address bus
	Set8BitAddress(address);
	
	//set Sram control pin low. we do this -AFTER- address is set because MBC2 latches to the address as soon as CS1 is set low. 
	//making the Set8BitAddress do nothing, it'll latch to the address it was set before the Set8BitAddress
	if(CS_Pin != 0)
		ClearPin(CTRL_PORT,CS_Pin);
	
	//set cartridge in read mode
	ClearPin(CTRL_PORT,RD);	
	
	GET_DATA(data);

	SetPin(CTRL_PORT,RD);
	if(CS_Pin != 0)
		SetPin(CTRL_PORT,CS_Pin);
	
	return data;
}
uint8_t ReadGBRamByte(uint16_t address)
{
	if(LoadedBankType == MBC_NONE || LoadedBankType == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	uint8_t ret = _Read8BitByte(CS1,address);
	
	//MBC2 only has the lower 4 bits as data, so we return it as 0xFx
	return (LoadedBankType == MBC2)?(0xF0 | ret ):ret;
}

inline void _Write8BitByte(uint8_t CS_Pin, uint16_t addr,uint8_t byte)
{	
	SetDataPinsAsOutput(); 
	SET_DATA(byte);
	Set8BitAddress(addr);
	
	//set Sram control pin low. we do this -AFTER- address is set because MBC2 latches to the address as soon as CS1 is set low. 
	//making the Set8BitAddress do nothing, it'll latch to the address it was set before the Set8BitAddress
	if(CS_Pin != 0)
		ClearPin(CTRL_PORT,CS_Pin);
	
	ClearPin(CTRL_PORT,WD);
	
	SetPin(CTRL_PORT,WD);
	if(CS_Pin != 0)
		SetPin(CTRL_PORT,CS_Pin);
	
	SetDataPinsAsInput();
	
	return;
}
int8_t WriteGBRamByte(uint16_t addr,uint8_t byte)
{
	if(LoadedBankType == MBC_NONE || LoadedBankType == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
		
	_Write8BitByte(CS1,addr,byte);
	return 1;
}

//--------------------------------------
//			GB Functions
//--------------------------------------
int8_t OpenGBRam(void)
{
	if(LoadedBankType == MBC_NONE || LoadedBankType == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);	
	
	if(LoadedBankType == MBC2)
	{
		//the ghost read fix from https://www.insidegadgets.com/2011/03/28/gbcartread-arduino-based-gameboy-cart-reader-%E2%80%93-part-2-read-the-ram/
		//he said it otherwise had issues with MBC2? :/
		ReadGBRomByte(0x0134);
	}
		
	//set banking mode to RAM
	if(LoadedBankType == MBC1)
		WriteGBRomByte(0x6000,0x01);
	
	//Init the MBC Ram!
	//uint16_t init_addr = 0x0000; 
	WriteGBRomByte(0x0000,0x0A);
	
	return 1;
}
void CloseGBRam(void)
{	
	uint8_t Bank_Type = LoadedBankType;
	//disable RAM again - VERY IMPORTANT -
	if(Bank_Type == MBC1)
		WriteGBRomByte(0x6000,0x00);
		
	//uint16_t init_addr = 0x0000;
	WriteGBRomByte(0x0000,0x00);

	return;
}
void SwitchROMBank(int8_t bank)
{	
	uint8_t Bank_Type = LoadedBankType;
	
	if(Bank_Type == MBC_NONE)
		return;

	//first some preperations
	uint16_t addr = 0x2100;
	uint16_t addr2 = 0x4000;
	 
	if(Bank_Type == MBC1)
		addr = 0x2000;
	else if(Bank_Type == MBC5)
		addr2 = 0x3000;
	
	switch(Bank_Type)
	{
		case MBC1:
			//in MBC1 we need to 
			// - set 0x6000 in rom mode 
			//WriteGBRomByte(0x6000,0);
			WriteGBRomByte(addr,bank & 0x1F);
			WriteGBRomByte(addr2,bank >> 5);
			break;
		case MBC2:
			WriteGBRomByte(addr,bank & 0x1F);
			break;
		case MBC5:
			WriteGBRomByte(addr,bank & 0xFF);
			WriteGBRomByte(addr2,bank >> 8);
			break;
		default:
		case MBC3:
			WriteGBRomByte(addr,bank);
			break;
	}

	return;
}
inline void SwitchRAMBank(int8_t bank)
{
	WriteGBRomByte(0x4000,bank);	
	return;
}
inline void SwitchFlashRAMBank(int8_t bank)
{
	WriteGBARamByte(0x5555,0xAA);
	WriteGBARamByte(0x2AAA,0x55);	
	WriteGBARamByte(0x5555,0xB0);
	WriteGBARamByte(0x0000,bank);
	return;
}



//---------------------------------------------------
//				GB Retrieval functions
//---------------------------------------------------

int8_t GetGBInfo(char* GameName, uint8_t* romFlag , uint8_t* ramFlag,uint8_t* cartFlag)
{
	if(GameName == NULL ||romFlag == NULL || ramFlag == NULL || cartFlag == NULL)
		return -1;
	
	//reset cart
	ClearPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,CS2);
	
	GBC_Header temp;
	uint8_t header[0x51] = {0};
	uint8_t FF_cnt = 0;	
	uint8_t Logo[0x30] = {
		0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
		0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
		0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
	};
	
	//code that reads everything...	
	//this saves alot of space, but at the cost of possible speed since we are forcing it to read everything
	for(uint8_t i = 0 ; i < ((sizeof(header) / sizeof(uint8_t))-1);i++)
	{
		header[i] = ReadGBRomByte(0x100+i);
		if(i >= 0 && i<=4 && header[i] == 0xFF)
		{
			FF_cnt++;
		}
		if(FF_cnt >= 3)
		{
			//data is just returning 0xFF (which is thx to the internal pullups). so no cart!
			return ERR_FAULT_CART;
		}
	}
	
	//read logo
	/*for(uint8_t i = 0 ; i < 0x30;i++)
	{
		header[_CALC_ADDR(_ADDR_LOGO+i)] = ReadGBRomByte(0x104+i);
		if(i >= 0 && i<=4 && header[_CALC_ADDR(_ADDR_LOGO+i)] == 0xFF)
		{
			FF_cnt++;
		}
		if(FF_cnt >= 3)
		{
			//data is just returning 0xFF (which is thx to the internal pullups). so no cart!
			return ERR_FAULT_CART;
		}
	}
	//read old licenseecode
	header[_CALC_ADDR(_ADDR_OLD_LICODE)] = ReadGBRomByte(_ADDR_OLD_LICODE);
	//read name
	for(uint8_t i = 0;i <= 16;i++)
	{
		header[_CALC_ADDR(_ADDR_NAME+i)] = ReadGBRomByte(_ADDR_NAME+i);
	}
	//read Cart Type
	header[_CALC_ADDR(_ADDR_CART_TYPE)] = ReadGBRomByte(_ADDR_CART_TYPE);
	//read rom&ram size
	header[_CALC_ADDR(_ADDR_ROM_SIZE)] = ReadGBRomByte(_ADDR_ROM_SIZE);
	header[_CALC_ADDR(_ADDR_RAM_SIZE)] = ReadGBRomByte(_ADDR_RAM_SIZE);*/
	
	/*
	temp.SGBSupported = header[_CALC_ADDR(_ADDR_SGB_SUPPORT)];
	temp.Region = header[_CALC_ADDR(_ADDR_REGION)];
	temp.RomVersion = header[_CALC_ADDR(_ADDR_ROM_VERSION)];
	
	//todo, verify header
	temp.HeaderChecksum = header[_CALC_ADDR(_ADDR_HEADER_CHECKSUM)];
	
	for(uint8_t i = 0;i < 2;i++)
	{
		temp.GlobalChecksum[i] = header[_CALC_ADDR(_ADDR_GLBL_CHECKSUM+i)];
	}
	temp.RomSizeFlag = header[_CALC_ADDR(_ADDR_ROM_SIZE)];
	temp.RamSize = header[_CALC_ADDR(_ADDR_RAM_SIZE)];	
	temp.MBCType = GetMBCType(temp.CartType);
	*/
	
	//compare logo!	
	for(int8_t i =0;i< 0x30;i++)
	{
		if(header[_CALC_ADDR(_ADDR_LOGO+i)] == Logo[i])
			continue;
		else
		{
			return ERR_LOGO_CHECK;
		}
	}

	uint8_t NameSize = 16;
	
	temp.OldLicenseeCode = header[_CALC_ADDR(_ADDR_OLD_LICODE)];
	//if its 0x33, we have a cart from past the SGB/GBC era. uses different mapping
	if(temp.OldLicenseeCode == 0x33)
	{
		//read the manufacturer code
		/*
			for(uint8_t i = 0;i < 4;i++)
			{
				temp.ManufacturerCode[i] = header[_CALC_ADDR(_ADDR_MANUFACT+i)];
			}
			
			for(uint8_t i = 0;i < 2;i++)
			{
				temp.NewLicenseeCode[i] = header[_CALC_ADDR(_ADDR_NEW_LICODE+i)];
			}	
		*/		
		temp.GBCFlag = header[_CALC_ADDR(_ADDR_GBC_FLAG)];
		NameSize = 11;
	}
	else
	{
		//we got an old fashion cart y0
		NameSize = 16;
		temp.GBCFlag = 0x00;
	}

	memset(GameName,0,17);
	for(uint8_t i = 0;i < NameSize;i++)
	{
		GameName[i] = header[_CALC_ADDR(_ADDR_NAME+i)];
	}
	
	temp.CartType = header[_CALC_ADDR(_ADDR_CART_TYPE)];
	*cartFlag = temp.GBCFlag;
	*romFlag = header[_CALC_ADDR(_ADDR_ROM_SIZE)];
	*ramFlag = header[_CALC_ADDR(_ADDR_RAM_SIZE)];	
	LoadedBankType = GetMBCType(temp.CartType);	
	return 1;	
}

uint16_t GetAmountOfRomBacks(uint8_t RomSizeFlag)
{
	if(RomSizeFlag <= 7)
		return 2 << RomSizeFlag;
	switch(RomSizeFlag)
	{
		case 52:
			return 72;
		case 53:
			return 80;
		case 54: 
			return 96;
		default:
			return 1;
	}
}
int8_t GetRamDetails(uint16_t *end_addr, uint8_t *banks,uint8_t RamSizeFlag)
{
	if(end_addr == NULL || banks == NULL)
		return ERR_MBC_SAVE_UNSUPPORTED;
	
	//every MBC type has RamSizeFlag as the amount of banks
	//...except MBC2 which needs RamSizeFlag to be set to 0, because its RAM is included in MBC2
	if(LoadedBankType != MBC2 && RamSizeFlag <= 0)
	{
		//no ram, BAIL IT
		return ERR_NO_INFO;
	}
	
	if(LoadedBankType == MBC2)
	{		
		//Set the Ram Size & end addr. MBC2 is euh...special :P
		*banks = 1;
		*end_addr = 0xA200;
	}
	else
	{
		*end_addr = 0xC000;
		switch(RamSizeFlag)
		{
			case 0x01: //1 bank of 2KB ram
				*end_addr = 0xA800;
			case 0x02: //1 bank of 8KB ram
				*banks = 1;
				break;
			case 0x03: //4 banks, 32KB
				*banks = 4;
				break;
			//these are undocumented to the wiki and the pdf 
			//but insidegaget had it in his source so euh... ok...
			case 0x04: //16 banks, 128KB said to be the gameboy camera
				*banks = 16;
				break;
			case 0x05: //8 banks, 64KB
				*banks = 8;
				break;
			default:
				return ERR_MBC_SAVE_UNSUPPORTED;
				break;
		}
	}
	return 1;
}
uint8_t GetMBCType(uint8_t CartType)
{
	uint8_t ret = MBC_NONE;
	
	switch(CartType)
	{
		case 0xFF: //HuC1 + RAM + BATTERY
		case 0x01: //MBC1
		case 0x02: //MBC1 + ROM
		case 0x03: //MBC1 + ROM + BATTERY
			ret = MBC1;
			break;
		
		case 0x05: //MBC2
		case 0x06: //MBC2 + BATTERY
			ret = MBC2;
			break;
			
		case 0x08: //ROM + RAM
		case 0x09: //ROM + RAM + BATTERY
		case 0x0B: //MMM01
		case 0x0C: //MMM01 + RAM
		case 0x0D: //MMM01 + RAM + BATTERY
		case 0xFC: //POCKET CAMERA
		case 0xFD: //BANDAI TAMA5
		case 0xFE: //HuC3
		case 0x0F: //MBC3 + TIMER + BATTERY
		case 0x10: //MBC3 + TIMER + RAM + BATTERY
		case 0x11: //MBC3
		case 0x12: //MBC3 + RAM
		case 0x13: //MBC3 + RAM + BATTERY
			ret = MBC3;
			break;
		
		case 0x15: //MBC4
		case 0x16: //MBC4 + RAM
		case 0x17: //MBC4 + RAM + BATTERY
			ret = MBC4;
			break;
			
		case 0x19: //MBC5
		case 0x1A: //MBC5 + RAM
		case 0x1B: //MBC5 + RAM + BATTERY
		case 0x1C: //MBC5 + RUMBLE
		case 0x1D: //MBC5 + RUMBLE + RAM
		case 0x1E: //MBC5 + RUMBLE + RAM + BATTERY
			ret = MBC5;
			break;
		
		case 0x00:
			ret = MBC_NONE;
			break;
		default:
			ret = MBC_UNSUPPORTED;
			break;	
	}
	
	return ret;
}
