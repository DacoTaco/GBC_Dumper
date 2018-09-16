/*
main - AVR code to use the GBC libraries to communicate with a GB/C cartridge
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

#ifdef SAVE_SPACE
#undef _ALLOW_NONE_API_CMD
#endif

//allow the NONE API commands
//#define _ALLOW_NONE_API_CMD

//API Commands!
#define API_READ_ROM "API_READ_ROM"
#define API_READ_ROM_SIZE 12
#define API_READ_RAM "API_READ_RAM"
#define API_READ_RAM_SIZE 12
#define API_WRITE_RAM "API_WRITE_RAM"
#define API_WRITE_RAM_SIZE 13

#include <inttypes.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "serial.h"
#include "mcp23008.h"
#include "gbc_error.h"
#include "gbc_api.h"
#include "GB_Cart.h"


uint8_t cmd_size = 0;
#define CMD_SIZE 0x21
char cmd[CMD_SIZE];
uint8_t process_cmd = 0;

/*

//things that are broken : 

*/

void ProcessCommand(void)
{
	process_cmd = 0;
	DisableSerialInterrupt();
	int8_t ret = API_GetGameInfo();
	if(ret > 0)
	{
		//this is the API version of reading the ROM or RAM
		if(strncmp(cmd,"API_READ_ROM",API_READ_ROM_SIZE) == 0 || strncmp(cmd,"API_READ_RAM",API_READ_RAM_SIZE) == 0 )
		{
			uint8_t ReadRom = (strncmp(cmd,"API_READ_ROM",API_READ_ROM_SIZE) == 0)?1:0;					
			ROM_TYPE type = TYPE_ROM;
			if(!ReadRom)
			{
				type = TYPE_RAM;
			}
			ret = API_Get_Memory(type);
		}
		//this is the API version of Writing the RAM
		else if(strncmp(cmd,"API_WRITE_RAM",API_WRITE_RAM_SIZE) == 0)
		{			
			ret = API_WriteRam();	
		}
#ifdef _ALLOW_NONE_API_CMD
		//all of the NONE API functions
		else if(
			(strncmp(cmd,"READROM",7) == 0) ||
			(strncmp(cmd,"READRAM",7) == 0) ||
			(strncmp(cmd,"WRITERAM",8) == 0)
		)
		{
			//the old, none API commands
			cprintf("checking cart...\r\n");
			cprintf("game inserted : %s\r\n",GameInfo.Name);
			cprintf("MBC Type : 0x%x\r\n",GetMBCType(GameInfo.CartType));
			cprintf("game banks : %u\r\n",GetRomBanks(GameInfo.RomSize));
	
			uint16_t banks = GameInfo.RamSize;
			if(banks > 0)
			{
				banks = 1024*2;
				for(uint8_t i = 1;i < GameInfo.RamSize;i++)
				{
					banks *= 4;
				}
			}
			cprintf("ram size : %u\r\n",banks);
			
			if(strncmp(cmd,"READROM",7) == 0)
			{
				cprintf("Dumping ROM...\r\n");
				DumpROM();
				cprintf("\r\ndone\r\n");
			}
			else if(strncmp(cmd,"READRAM",7) == 0)
			{
				cprintf("Dumping RAM...\r\n");
				if(GameInfo.RamSize > 0 || GameInfo.MBCType == MBC2 && GameInfo.RamSize == 0)
				{
					DumpRAM();
				}
				cprintf("\r\ndone\r\n");
			}
			else if(strncmp(cmd,"WRITERAM",8) == 0)
			{
				WriteRAM();
				cprintf("\r\ndone\r\n");
			}
		}
#endif
		else
		{
			API_Send_Abort(API_ABORT_ERROR);
			cprintf("COMMAND '");
			cprintf(cmd);
			cprintf("' UNKNOWN\r\n");
		}
	}
	
	//process errors
	if(ret < 0)
	{
		//if there is no Gameinfo/Cart detected that means we are here from cart detection. therefor send an abort.
		//else, just process. it has send its own abort!
		if(!API_CartInserted())
			API_Send_Abort(API_ABORT_ERROR);
		
		switch(ret)
		{
			case ERR_PACKET_FAILURE:
				cprintf("PCKT_FAILURE");
				break;
			case ERR_NOK_RETURNED:
				cprintf("NOK_RET");
				break;
			case ERR_NO_SAVE:
				cprintf("NO_SAV");
				break;
			case ERR_LOGO_CHECK:
				cprintf("LOGO_CHECK\r\n");
				break;
			case ERR_FAULT_CART:
				cprintf("FAULT_CART\r\n");
				break;
			default :
				cprintf("ERR_UNKNOWN :'");
				cprintf_char(ret);
				cprintf("'\r\n");
				break;
		}		
		goto end_function;
	}
	
end_function:
	cmd_size = 0;
	memset(cmd,0,CMD_SIZE);
	EnableSerialInterrupt();
	return;
}
void ProcessChar(char byte)
{
	if(byte == API_HANDSHAKE_REQUEST)
	{
		cprintf_char(API_HANDSHAKE_ACCEPT);
		return;
	}
	else if(byte == '\n' || byte == '\r')
	{
		if(cmd_size > 0)
			//set variable instead of processing command. something in the lines of disabling interrupt while in one that just doesn't work out nicely
			//ProcessCommand();
			process_cmd = 1;
		return;
	}
	else if( 
		(cmd_size+1) < ((sizeof(cmd) / sizeof(uint8_t))-1)
	)
	{
		//space left!
		cmd[cmd_size] = byte;
		cmd_size++;
	}
	
	//cprintf_char(byte);
	return;
}
int main(void)
{
	//fire up the usart
	initConsole();
	
	//setup the pins!
	SetupPins();

	//set it so that incoming msg's are ignored.
	setSerialRecvCallback(ProcessChar);
	
/*#ifdef __ATMEGA8__
	cprintf("Atmega8 says : ");
#elif defined(__ATMEGA32__)
	cprintf("Atmega32 says : ");
#endif*/

	cprintf("Ready\r\n");

    // main loop
	// do not kill the loop. despite the console/UART being set as interrupt. going out of main kills the program completely
	//uint16_t addr = 0x104;//0x200;//0x8421;
    while(1) 
	{
		if(process_cmd)
		{
			ProcessCommand();
		}
		if(CheckControlPin(BTN) == LOW)
		{
			/*cprintf("Btn pressed!\n\r");
			/*uint8_t data = 0;
			data = ReadByte(addr);
			cprintf("data : ");
			cprintf_char(data);	
			cprintf("\n\r");

			SetControlPin(RD,HIGH);	
			if(CheckControlPin(RST) == 1)
			{
				SetControlPin(RST,LOW);
			}*/
			
			/*cmd[0] = 'T';
			cmd[1] = 'E';
			cmd[2] = 'S';
			cmd[3] = 'T';
			cmd_size = 4;
			cprintf("processing command...\n\r");
			ProcessCommand();
			//addr++;
			_delay_ms(200);*/
		}
    }
}