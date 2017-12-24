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
#include <util/delay.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/eeprom.h>
#include "serial.h"
#include "gbc_error.h"
#include "gbc_api.h"
#include "GB_Cart.h"

uint8_t cmd_size = 0;
#define CMD_SIZE 0x21
char cmd[CMD_SIZE];
volatile uint8_t cmd_busy = 0;
/*

//things that are broken : 

*/

void ProcessCommand(void)
{
	cmd_busy = 1;
	int8_t ret = API_GetGameInfo();
	if(ret > 0)
	{
		//this is the API version of reading the ROM or RAM
		if(strncmp(cmd,"API_READ_ROM",12) == 0 || strncmp(cmd,"API_READ_RAM",12) == 0 )
		{
			uint8_t ReadRom = (strncmp(cmd,"API_READ_ROM",12) == 0)?1:0;					
			ROM_TYPE type = TYPE_ROM;
			if(!ReadRom)
			{
				type = TYPE_RAM;
			}
			API_Get_Memory(type);
		}
		//this is the API version of Writing the RAM
		else if(strncmp(cmd,"API_WRITE_RAM",13) == 0)
		{			
			API_WriteRam();	
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
				if(GameInfo.RamSize > 0)
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
	else
	{
		API_Send_Abort(API_ABORT_ERROR);
		switch(ret)
		{
			case ERR_LOGO_CHECK:
				cprintf("ERR_LOGO_CHECK\r\n");
				break;
			case ERR_FAULT_CART:
				cprintf("ERR_FAULT_CART\r\n");
				break;
			default :
				cprintf("ERR_UNKNOWN\r\n");
				/*cprintf("ERR_UNKNOWN_");
				cprintf(ret);
				cprintf("\r\n");*/
				break;
		}
		
		goto end_function;
	}
	
end_function:
	cmd_busy = 0;
	cmd_size = 0;
	memset(cmd,0,CMD_SIZE);
	return;
}
void ProcessChar(char byte)
{
	//ignore incoming data when busy
	if(cmd_busy == 1)
		return;
	
	if(byte == API_HANDSHAKE_REQUEST)
	{
		cprintf_char(API_HANDSHAKE_ACCEPT);
		return;
	}
	else if(cmd_size > 0 && (byte == '\n' || byte == '\r') )
	{
		cprintf("\r\n");
		ProcessCommand();
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
	
	cprintf_char(byte);
	return;
}
int main(void)
{
	//setup the pins!
	SetupPins();
	
    // fire up the usart
	initConsole();

	//set it so that incoming msg's are ignored.
	setRecvCallback(ProcessChar);
	
	cprintf("Ready\r\n");
    // main loop
	// do not kill the loop. despite the console/UART being set as interrupt. going out of main kills the program completely
    while(1) 
	{
		/*if((PIND & 0b01000000)==0)
		{
		
		}*/
    }
}