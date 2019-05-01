/*
gb_api - AVR Code/API to access the GBA/GBC library to communicate with a GBA/GB(C) cartridge. accessing ROM & RAM
Copyright (C) 2016-2017  DacoTaco
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
#include "serial.h"
#include "gb_error.h"
#include "gb_pins.h"
#include "8bit_cart.h"
#include "24bit_cart.h"
#include "gbc_api.h"


typedef struct _api_info
{
	char Name[18];
	uint8_t RomSizeFlag;
	uint8_t RamSize;
	uint32_t fileSize;
	uint8_t CartFlag;
} api_info;
api_info gameInfo; 

uint8_t _gba_cart = 0;

void API_Init(void)
{
	API_SetupPins(1);
	API_ResetGameInfo();
}
void API_SetupPins(int8_t _gba_mode)
{
	if(_gba_mode == _gba_cart)
		return;
	
	_gba_cart = _gba_mode;
	
	if(_gba_cart)
	{
		Setup_Pins_24bitMode();
	}
	else
	{
		Setup_Pins_8bitMode();
	}
	return;
}
int8_t API_WaitForOK(void)
{
	//disable interrupts, like serial interrupt for example :P 
	//we will handle the data, kthxbye
	DisableSerialInterrupt();
	int8_t ret = 0;
	
	cprintf_char(API_OK);
	uint8_t response = Serial_ReadByte();
	
	switch(response)
	{
		default:
		case API_NOK:
			ret = 0;
			break;
		case API_ABORT:
			ret = -1;
			break;
		case API_OK:
			ret = 1;
			break;
	}
	
	//EnableSerialInterrupt();
	return ret;
}
void API_ResetGameInfo(void)
{
	//reset GameInfo
	//code only checks byte 0 of the name. invalidate that and its ok :P
	//this produces smaller code too xD
	gameInfo.Name[0] = 0xFF;
}
int8_t API_GetGameInfo(void)
{
	if(gameInfo.Name[0] != 0xFF)
		return 1;
	int8_t ret = 0;
	
	if(_gba_cart)
	{
		ret = GetGBAInfo(gameInfo.Name,&gameInfo.CartFlag);
	}
	else
	{
		ret = GetGBInfo(gameInfo.Name,&gameInfo.RomSizeFlag,&gameInfo.RamSize,&gameInfo.CartFlag);
	}
	
	if(ret > 0 && gameInfo.Name[0] != 0xFF)
		return 1;
	
	API_ResetGameInfo();
	return ret;
}
int8_t API_Get_Memory(ROM_TYPE type,int8_t _gbaMode)
{	
	API_SetupPins(_gbaMode);
	
	int8_t ret = API_GetGameInfo();
	if(ret < 1)
	{
		API_Send_Abort(API_ABORT_CMD);
		return ret;
	}
			
	if(type == TYPE_ROM)
	{
		if(_gba_cart)
		{
			gameInfo.fileSize = GetGBARomSize();
		}
		else
		{
			gameInfo.fileSize = GetAmountOfRomBacks(gameInfo.RomSizeFlag) * 0x4000UL;
		}		
	}
	else
	{
		if(_gba_cart)
		{
			gameInfo.fileSize = GetGBARamSize(&gameInfo.CartFlag);
			if(gameInfo.fileSize <= 0)
			{
				API_Send_Abort(API_ABORT_CMD);
				return ERR_NO_SAVE;
			}
		}
		else
		{
			uint16_t end_addr = 0;
			uint8_t _banks;
			
			if(GetRamDetails(&end_addr, &_banks,gameInfo.RamSize) < 0)
			{
				//ram error, BAIL IT
				API_Send_Abort(API_ABORT_CMD);
				return ERR_NO_SAVE;
			}
			
			if(end_addr < 0xC000)
			{
				gameInfo.fileSize = end_addr - 0xA000;
			}
			else
			{
				gameInfo.fileSize = 0x2000 * _banks;
			}
		}		
	}
	
	API_Send_Cart_Type();
	API_Send_Name();
	API_Send_Size();	
	
	if(API_WaitForOK() <= 0)
	{
		API_Send_Abort(API_ABORT_PACKET);
		return ERR_NOK_RETURNED;
	}
	
	if(type == TYPE_RAM)
	{
		return API_GetRam();
	}
	else
	{
		return API_GetRom();
	}
}
int8_t API_WriteGBRam(void)
{	
	//reset game cart. this causes all banks & states to reset
	ClearPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,CS2);
	
	gameInfo.fileSize = 0;
	int8_t ret = 0;
	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000;
	uint8_t banks = 0;
	
	if(GetRamDetails(&end_addr,&banks,gameInfo.RamSize) < 0)
	{	
		API_Send_Abort(API_ABORT_CMD);
		ret = ERR_NO_SAVE;
		goto end_function;
	}
		
	//precheck and send everything
	//for WRITERAM we need to send Ram size, wait for the OK(0x80) or NOK(anything NOT 0x80) signal, and then start receiving.
	if(end_addr < 0xC000)
	{
		gameInfo.fileSize = end_addr - 0xA000;
	}
	else
	{
		gameInfo.fileSize = 0x2000 * banks;
	}
	
	API_Send_Size();
	if(API_WaitForOK() <= 0)
	{
		API_Send_Abort(API_ABORT_CMD);
		ret = ERR_NOK_RETURNED;
		goto end_function;
	}
	
	
	//we got the OK!	
	OpenGBRam();
	
	
	
	/*
	//this function will look as following : 
	//PC will be waiting for a TASK_START
	//Send TASK_START and enter for loop.
	//wait for data to be send. we are expecting an API_OK followed by the byte we need to write
	//Write the data, and send an API_VERIFY with the READ BYTE at the location
	//wait for 2 bytes. 
	//if we get an OK + byte -> use that byte and write it to the next address
	//if we get an NOK + byte -> use that byte to write to the same address again, and send a VERIFY again
	//if we get different data then expected, ABBANDON SHIP!!
	*/
	
	//send the Start, we are ready for loop!
	cprintf_char(API_TASK_START);
	
	uint8_t data_recv[2];
	uint8_t bank = 0;
	ret = 1;
	
	//switch bank!
	if(LoadedBankType != MBC2)
		SwitchRAMBank(bank);
	
	//disable serial interrupt. we will handle the data, kthxbye
	DisableSerialInterrupt();
	
	//we start our loop at addr -1 because we will add 1 asa we start the loop
	for(uint16_t i = addr-1;i< end_addr;)
	{		
		//receive first byte
		while ( !(UCSRA & (_BV(RXC))) );	
		//add the delay because the while tends to exit once in a while to early and it makes us retrieve the wrong byte. for example 0xFA often tended to become 00.
		asm ("nop");
		data_recv[0] = UDR;
		
		//second byte
		while ( !(UCSRA & (_BV(RXC))) );	
		asm ("nop");
		data_recv[1] = UDR;
		
		//check wether we got an OK signal (meaning we can start to write or data written is OK
		if(data_recv[0] == API_OK)
		{
			//go to next address!
			i++;
			//if we are at the end of our RAM address, and there are more banks, go to next bank!
			if(i >= end_addr && bank < banks)
			{
				bank++;
				i = addr;
				if(LoadedBankType != MBC2)
					SwitchRAMBank(bank);	
			}
			
			//if we have written everything , on all banks , gtfo. we are done
			if(i >= end_addr || bank >= banks)
			{
				break;
			}
			
			//Write and read written byte for verification
			WriteGBRamByte(i,data_recv[1]);
			uint8_t data = ReadGBRamByte(i);
			
			cprintf_char(API_VERIFY);
			cprintf_char(data);
		}
		else if(data_recv[0] == API_NOK)
		{
			//data was decided NOT OK, we go back and retry!
			WriteGBRamByte(i,data_recv[1]);			
			uint8_t data = ReadGBRamByte(i);
			cprintf_char(API_VERIFY);
			cprintf_char(data);
		}
		else
		{
			//we received an abort or invalid command. QUIT!!
			ret = ERR_PACKET_FAILURE;
			cprintf_char(API_ABORT);
			cprintf_char(API_ABORT_PACKET);
			goto end_function;
		}
	}
	//we are finished, lets send that!
	cprintf_char(API_TASK_FINISHED);
	
	CloseGBRam();
end_function:
	//re-enable interrupts!
	ClearPin(CTRL_PORT,CS2);
	API_ResetGameInfo();
	EnableSerialInterrupt();
	return ret;
}
int8_t API_WriteRam(int8_t _gbaMode)
{		
	API_SetupPins(_gbaMode);
	int8_t ret = API_GetGameInfo();
	if(!ret)
	{
		API_Send_Abort(API_ABORT_CMD);
		return ret;
	}
	
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	SetPin(CTRL_PORT,CS2);
	
	if(_gba_cart)
	{
		return -1;
	}
	else
	{
		return API_WriteGBRam();
	}
}
int8_t API_GetRom(void)
{
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	SetPin(CTRL_PORT,CS2);
		
	if(_gba_cart)
	{			
		//for dumping we use the GBA's increment reading mode. this saves a alot of cycles and is therefor a lot faster
		//see the function for more info.
		uint32_t _readSize = gameInfo.fileSize / 2;		
		for(uint32_t i = 0; i < _readSize;i++)
		{
			//GBA rom's only latch the lower 16bits of the address and increments from that
			//this means that every 0x10000(0x20000 in file) we need to send an actual read command so it relatches the address
			uint16_t data = Read24BitIncrementedBytes((i%0x10000) == 0?1:0,i);
			cprintf_char((uint8_t)data & 0xFF);
			cprintf_char((data >> 8) & 0xFF);
		}
		SetPin(CTRL_PORT,CS1);
	}
	else
	{
		//reset cart
		ClearPin(CTRL_PORT,CS2);
		SetPin(CTRL_PORT,CS2);
		uint16_t banks = GetAmountOfRomBacks(gameInfo.RomSizeFlag);
		
		uint16_t addr = 0;
		for(uint16_t bank = 1;bank < banks;bank++)
		{
			SwitchROMBank(bank);
			for(;addr < 0x8000;addr++)
			{
				cprintf_char(ReadGBRomByte(addr));
			}
			addr = 0x4000;
		}			
	}	
	
	API_ResetGameInfo();
	return 1;
}
int8_t API_GetRam(void)
{
	//reset game cart. this causes all banks & states to reset
	if(!_gba_cart)	
	{
		if(LoadedBankType == MBC_NONE || LoadedBankType == MBC_UNSUPPORTED)
			return ERR_NO_MBC;
		ClearPin(CTRL_PORT,CS2);
	}
	
	SetPin(CTRL_PORT,WD);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	SetPin(CTRL_PORT,CS2);
	
	if(_gba_cart)	
	{
		Setup_Pins_8bitMode();
		uint8_t bank = 0;
		for( uint32_t i = 0; i < gameInfo.fileSize;i++)
		{
			if((i%0x10000) == 0 && gameInfo.CartFlag == GBA_SAVE_FLASH)
			{
				SwitchFlashRAMBank(bank);
				bank++;
			}
			
			cprintf_char(ReadGBARamByte(i & 0xFFFF));
		}
		Setup_Pins_24bitMode();
	}
	else
	{
		//read RAM Address'
		uint16_t addr = 0xA000;
		uint16_t end_addr = 0xC000; //actually ends at 0xBFFF
		uint8_t banks = 0;
		
		int8_t ret = GetRamDetails(&end_addr,&banks,gameInfo.RamSize);
		
		if(ret < 0)
			return ERR_NO_SAVE;
		
		OpenGBRam();

		for(uint8_t bank = 0;bank < banks;bank++)
		{
			//if we aren't dealing with MBC2, set bank to 0!
			if(LoadedBankType != MBC2)
				SwitchRAMBank(bank);	
			
			for(uint16_t i = addr;i< end_addr ;i++)
			{
				cprintf_char(ReadGBRamByte(i));
				//asm("nop");	
			}
		}
		
		CloseGBRam();	
		ClearPin(CTRL_PORT,CS2);
	}
	
	SetPin(CTRL_PORT,CS2);
	API_ResetGameInfo();
	return 1;
}
void API_Send_Abort(uint8_t type)
{
	cprintf_char(API_ABORT);
	cprintf_char(type);
	API_ResetGameInfo();
}

void API_Send_Name(void)
{
	cprintf_char(API_GAMENAME_START);
	cprintf_char(strnlen(gameInfo.Name,17));
	cprintf_char(API_GAMENAME_END);
	cprintf(gameInfo.Name);
	return;
}

void API_Send_Size(void)
{
	cprintf_char(API_FILESIZE_START);
	cprintf_char((gameInfo.fileSize >> 24) & 0xFF);
	cprintf_char((gameInfo.fileSize >> 16) & 0xFF);
	cprintf_char((gameInfo.fileSize >> 8) & 0xFF);
	cprintf_char(gameInfo.fileSize & 0xFF);
	cprintf_char(API_FILESIZE_END);
	return;
}

void API_Send_Cart_Type(void)
{
	uint8_t toSend = 0xFF;
	
	if(_gba_cart)
		toSend = API_GBA_ONLY;
	else
	{	
		switch(gameInfo.CartFlag)
		{
			case 0x80:
				toSend = API_GBC_HYBRID;
				break;
			case 0xC0:
				toSend = API_GBC_ONLY;
				break;
			case 0x00:
				toSend = API_GBC_GB;
				break;
		}
	}
	
	cprintf_char(API_GB_CART_TYPE_START);
	cprintf_char(toSend);
	cprintf_char(API_GB_CART_TYPE_END);
}