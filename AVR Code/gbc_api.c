/*
gbc_api - AVR Code/API to access the GBC library to communicate with a GB/C cartridge. accessing ROM & RAM
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
#include "gbc_api.h"
#include "serial.h"
#include "GB_Cart.h"
#include "gbc_error.h"

int8_t API_WaitForOK(void)
{
	//disable interrupts, like serial interrupt for example :P 
	//we will handle the data, kthxbye
	cli();
	int8_t ret = 0;
	
	cprintf_char(API_OK);
	_delay_us(300);
	uint8_t response = Serial_GetByte();
	
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
	
	sei();
	return ret;
}
void API_ResetGameInfo(void)
{
	//reset GameInfo
	//memset(&GameInfo,0,sizeof(CartInfo));
	//code only checks byte 0 of the name. invalidate that and its ok :P
	//this produces smaller code too xD
	GameInfo.Name[0] = 0x00;
}
int8_t API_GetGameInfo(void)
{
	int8_t ret = 0;
	SetControlPin(RST,HIGH);
	ret = GetGameInfo();
	SetControlPin(RST,LOW);
	return ret;
}
int8_t API_GotCartInfo(void)
{
	if(GameInfo.Name[0] == 0x00)
	{
		int8_t ret = API_GetGameInfo();
		if(ret <= 0)
		{
			return ret;
		}
		return 1;
	}
	return 1;
}
int8_t API_CartInserted(void)
{
	return API_GotCartInfo();
}
void API_Get_Memory(ROM_TYPE type)
{
	if(!API_GotCartInfo())
	{
		API_Send_Abort(API_ABORT_CMD);
		return;
	}
	
	uint16_t fileSize = 0;
			
	if(type == TYPE_ROM)
	{
		//because rom sizes are to big, we will just pass the banks amount
		fileSize = GetRomBanks(GameInfo.RomSize);
	}
	else
	{
		if(GameInfo.RamSize == 0)
		{
			//no ram, BAIL IT
			API_Send_Abort(API_ABORT_CMD);
			return;
		}
		uint16_t end_addr = 0;
		uint8_t _banks;
		GetRamDetails(GameInfo.MBCType, &end_addr, &_banks,GameInfo.RamSize);
		if(end_addr < 0xC000)
		{
			fileSize = end_addr - 0xA000;
		}
		else
		{
			fileSize = 0x2000 * _banks;
		}
	}
	
	API_Send_Name(GameInfo.Name);
	API_Send_Size(fileSize);
	API_Send_GBC_Support(GameInfo.GBCFlag);
	
	if(API_WaitForOK() > 0)
	{
		if(type == TYPE_RAM)
		{
			API_GetRam();
		}
		else
		{
			API_GetRom();
		}
	}
	else
	{
		API_Send_Abort(API_ABORT_PACKET);
	}
}
void API_WriteRam(void)
{		
	if(!API_GotCartInfo())
	{
		API_Send_Abort(API_ABORT_CMD);
		return;
	}
	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
	
	//reset game cart. this causes all banks & states to reset
	SetControlPin(RST,LOW);
	_delay_us(50);
	SetControlPin(RST,HIGH);
	
	if(GameInfo.RamSize == 0)
	{
		//no ram, BAIL IT
		API_Send_Abort(API_ABORT_CMD);
		goto end_function;
	}

	uint16_t fileSize = 0;
	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000;
	uint8_t banks = 0;
	uint8_t Bank_Type = GameInfo.MBCType;
	
	if(GetRamDetails(Bank_Type,&end_addr,&banks,GameInfo.RamSize) < 0)
	{	
		API_Send_Abort(API_ABORT_CMD);
		goto end_function;
	}
		
	//precheck and send everything
	//for WRITERAM we need to send Ram size, wait for the OK(0x80) or NOK(anything NOT 0x80) signal, and then start receiving.
	if(end_addr < 0xC000)
	{
		fileSize = end_addr - 0xA000;
	}
	else
	{
		fileSize = 0x2000 * banks;
	}
	
	API_Send_Size(fileSize);
	if(API_WaitForOK() <= 0)
	{
		API_Send_Abort(API_ABORT_CMD);
		goto end_function;
	}
	
	
	//we got the OK!	
	OpenRam();
	
	//send the Start, we are ready for loop!
	cprintf_char(API_TASK_START);
	/*cprintf_char((addr >> 8) & 0xFF);
	cprintf_char(addr & 0xFF);
	cprintf_char((end_addr >> 8) & 0xFF);
	cprintf_char(end_addr & 0xFF);	*/	
	/*this will look as following : 
	//the handshake is done and the PC is waiting for the START!
	//this function will look as following : 

	//Send TASK_START and enter for loop.
	//wait for data to be send. we are expecting an API_OK followed by the byte we need to write
	//Write the data, and send an API_VERIFY with the READ BYTE at the location
	//wait for 2 bytes. 
	//if we get an OK + byte -> use that byte and write it to the next address
	//if we get an NOK + byte -> use that byte to write to the same address again, and send a VERIFY again
	//if we get different data then expected, ABBANDON SHIP!!
	*/
	
	uint8_t data_recv[2];
	uint8_t bank = 0;
	
	//switch bank!
	if(Bank_Type != MBC2)
		SwitchRAMBank(bank,Bank_Type);
	
	//disable interrupts, like serial interrupt for example :P 
	//we will handle the data, kthxbye
	cli();
	//we start our loop at addr -1 because we will add 1 asa we start the loop
	for(uint16_t i = addr-1;i< end_addr;)
	{		
		//receive first byte
		//wait untill we get data...
		while ( !(UCSRA & (_BV(RXC))) );	
		//add the delay because the while tends to exit once in a while to early and it makes us retrieve the wrong byte. for example 0xFA often tended to become 00. used to be 5ms
		_delay_us(30);
		data_recv[0] = UDR;
		
		//second byte
		while ( !(UCSRA & (_BV(RXC))) );	
		_delay_us(30);
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
				if(Bank_Type != MBC2)
					SwitchRAMBank(bank,Bank_Type);	
			}
			
			//if we have written everything , on all banks , gtfo. we are done
			if(i >= end_addr || bank >= banks)
			{
				break;
			}
			
			//Write and read written byte for verification
			WriteRAMByte(i,data_recv[1],Bank_Type);
			uint8_t data = GetRAMByte(i,Bank_Type);
			
			cprintf_char(API_VERIFY);
			cprintf_char(data);
		}
		if(data_recv[0] == API_NOK)
		{
			//data was decided NOT OK, we go back and retry!
			WriteRAMByte(i,data_recv[1],Bank_Type);
			uint8_t data = GetRAMByte(i,Bank_Type);
			cprintf_char(API_VERIFY);
			cprintf_char(data);
			/*cprintf_char((i >> 8) & 0xFF);
			cprintf_char(i & 0xFF);
			cprintf_char(bank);
			cprintf_char(GetRAMByte(i+1,Bank_Type));*/
		}
		else if(data_recv[0] == API_ABORT)
		{
			//we received an abort. QUIT!!
			cprintf_char(API_ABORT);
			cprintf_char(API_ABORT_PACKET);
			break;
		}
		else if(data_recv[0] != API_OK && data_recv[0] != API_NOK)
		{
			//data was not received well or not supported. ask for a resend
			cprintf_char(API_RESEND_CMD);				
		}
	}
	//we are finished, lets send that!
	cprintf_char(API_TASK_FINISHED);
	
	CloseRam();
end_function:
	//re-enable interrupts!
	SetControlPin(RST,LOW);
	API_ResetGameInfo();
	sei();
	return;
}
int8_t API_GetRom(void)
{
	SetControlPin(RST,HIGH);
	
	//DumpROM();
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);	
	
	if(!API_GotCartInfo())
	{
		API_Send_Abort(API_ABORT_CMD);
		return ERR_NO_INFO;
	}
	
	uint16_t banks = GetRomBanks(GameInfo.RomSize);
	
	for(uint16_t bank = 1;bank < banks;bank++)
	{
		uint16_t addr = 0x4000;
		SwitchROMBank(bank,GameInfo.MBCType);
		if(bank <= 1)
		{
			addr = 0x00;
		}

		for(;addr < 0x8000;addr++)
		{
			cprintf_char(GetByte(addr));
		}
	}
	
	SetControlPin(RST,LOW);
	API_ResetGameInfo();
	return 1;
}
int8_t API_GetRam(void)
{
	SetControlPin(RST,HIGH);
	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
	
	//reset game cart. this causes all banks & states to reset
	SetControlPin(RST,LOW);
	_delay_us(50);
	SetControlPin(RST,HIGH);
	
	if(!API_GotCartInfo())
	{
		API_Send_Abort(API_ABORT_CMD);
		return ERR_NO_INFO;
	}
	
	uint8_t Bank_Type = GameInfo.MBCType;
	
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
		
	if(GameInfo.RamSize <= 0)
		return ERR_NO_SAVE;

	
	//read RAM Address'
	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000; //actually ends at 0xBFFF
	uint8_t banks = 0;
	
	int8_t ret = GetRamDetails(Bank_Type,&end_addr,&banks,GameInfo.RamSize);
	
	if(ret < 0)
		return ret;
		
	OpenRam();

	for(uint8_t bank = 0;bank < banks;bank++)
	{
		//if we aren't dealing with MBC2, set bank to 0!
		if(Bank_Type != MBC2)
			SwitchRAMBank(bank,Bank_Type);
			
		for(uint16_t i = addr;i< end_addr ;i++)
		{
			cprintf_char(GetRAMByte(i,Bank_Type));
			_delay_us(500);
		}
	}
	
	CloseRam();	
	SetControlPin(RST,LOW);
	API_ResetGameInfo();
	return 1;
}
void API_Send_Abort(uint8_t type)
{
	cprintf_char(API_ABORT);
	cprintf_char(type);
	API_ResetGameInfo();
}

void API_Send_Name(char* Name)
{
	cprintf_char(API_GAMENAME_START);
	cprintf_char(strnlen(Name,17));
	cprintf_char(API_GAMENAME_END);
	cprintf(Name);
	return;
}

void API_Send_Size(uint16_t size)
{
	cprintf_char(API_FILESIZE_START);
	cprintf_char(size >> 8);
	cprintf_char(size & 0xFF);
	cprintf_char(API_FILESIZE_END);
	return;
}
void API_Send_GBC_Support(uint8_t GBCFlag)
{
	uint8_t toSend;
	switch(GBCFlag)
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
		default:
			toSend = API_GBC_GB;
			break;
	}
	cprintf_char(API_GBC_SUPPORT_START);
	cprintf_char(toSend);
	cprintf_char(API_GBC_SUPPORT_END);
}