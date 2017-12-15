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

void API_Get_Memory(ROM_TYPE type,char* Name,uint16_t size)
{
	API_Send_Name(Name);
	API_Send_Size(size);
	
	if(type == TYPE_RAM)
	{
		API_GetRam();
	}
	else
	{
		API_GetRom();
	}
}
int8_t API_WaitForOK(void)
{
	//disable interrupts, like serial interrupt for example :P 
	//we will handle the data, kthxbye
	cli();
	int8_t ret = 0;
	
	cprintf_char(API_OK);
	uint8_t response = Serial_GetByte();
	
	if(response == API_OK)
		ret = 1;
	else if(response == API_NOK)
	{
		ret = 0;
	}
	else if(response >= API_ABORT)
	{	
		//an abort was the response!
		ret = -1;
	}
	
	sei();
	return ret;
}
int8_t API_WriteRam(void)
{
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
	
	//reset game cart. this causes all banks & states to reset
	SetControlPin(RST,LOW);
	_delay_us(50);
	SetControlPin(RST,HIGH);
	int8_t ret = 1;
	
	//disable interrupts, like serial interrupt for example :P 
	//we will handle the data, kthxbye
	cli();
	
	if(GameInfo.Name[0] == 0x00)
	{
		//header isn't loaded yet!
		ret = GetGameInfo();
		if(ret <= 0)
		{	
			goto end_function;
		}
	}

	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000;
	uint8_t banks = 0;
	uint8_t Bank_Type = GetMBCType(GameInfo.CartType);
	
	ret = GetRamDetails(Bank_Type,&end_addr,&banks,GameInfo.RamSize);
	if(ret < 0)
		goto end_function;
	
	OpenRam();
	
	//send the Start, we are ready for loop!
	cprintf_char(API_TASK_START);	
			
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
				i = addr -1;
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
	sei();
	return ret;
}
int8_t API_GetRom(void)
{
	DumpROM();
	return 1;
}
int8_t API_GetRam(void)
{
	DumpRAM();
	return 1;
}
void API_Send_Abort(uint8_t type)
{
	cprintf_char(API_ABORT);
	cprintf_char(type);
}

void API_Send_Name(char* Name)
{
	cprintf_char(API_GAMENAME_START);
	cprintf_char(strnlen(Name,17));
	cprintf_char(API_GAMENAME_END);
	cprintf("%s",Name);
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