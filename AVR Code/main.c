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
#include "GB_Cart.h"
#include "gbc_api.h"

#define SetPin(Port, Bit)    Port |= (1 << Bit)
#define ClearPin(Port, Bit)    Port &= ~(1 << Bit)

//#define _ALLOW_NONE_API_CMD

uint8_t cmd_size = 0;
char cmd[0x21];
uint8_t cmd_ready = 0;
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
			uint16_t fileSize = 0;
			
			if(ReadRom)
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
				}
				uint16_t end_addr = 0;
				uint8_t _banks;
				GetRamDetails(GameInfo.CartType, &end_addr, &_banks,GameInfo.RamSize);
				if(end_addr < 0xC000)
				{
					fileSize = end_addr - 0xA000;
				}
				else
				{
					fileSize = 0x2000 * _banks;
				}
			}
			
			ROM_TYPE type = TYPE_ROM;
			if(!ReadRom)
			{
				type = TYPE_RAM;
			}
			API_Get_Memory(type,GameInfo.Name,fileSize);
		}
		//this is the API version of Writing the RAM
		else if(strncmp(cmd,"API_WRITE_RAM",13) == 0)
		{
			//for WRITERAM we need to send Ram size, wait for the OK(0x80) or NOK(anything NOT 0x80) signal, and then start receiving.
			uint16_t fileSize = 0;
			if(GameInfo.RamSize == 0)
			{
				//no ram, BAIL IT
				API_Send_Abort(API_ABORT_CMD);
				goto end_function;
			}
			uint16_t end_addr = 0;
			uint8_t _banks;
			GetRamDetails(GameInfo.CartType, &end_addr, &_banks,GameInfo.RamSize);
			if(end_addr < 0xC000)
			{
				fileSize = end_addr - 0xA000;
			}
			else
			{
				fileSize = 0x2000 * _banks;
			}
			
			API_Send_Size(fileSize);
			if(API_WaitForOK())
			{
				//we got the OK!
				//cprintf("We got the OK!\r\n");
				API_WriteRam();
			}
			else
			{
				API_Send_Abort(API_ABORT_CMD);
				goto end_function;
			}
			
			
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
			cprintf("game bank amount : %u\r\n",GetRomBanks(GameInfo.RomSize));
	
			uint16_t banks = GameInfo.RamSize;
			if(banks > 0)
			{
				banks = 1024*2;
				for(uint8_t i = 1;i < GameInfo.RamSize;i++)
				{
					banks *= 4;
				}
			}
			cprintf("game ram size : %u\r\n",banks);
			
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
				//cprintf("RAM WRITING UNSUPPORTED ATM\r\n");
				WriteRAM();
				cprintf("\r\ndone\r\n");
			}
		}
#endif
		else
		{
			API_Send_Abort(API_ABORT_ERROR);
			cprintf("COMMAND '%s' UNKNOWN\r\n",cmd);
		}
	}
	else
	{
		API_Send_Abort(API_ABORT_ERROR);
		switch(ret)
		{
			case ERR_FAULT_CART:
				cprintf("Error Reading game : ERR_FAULT_CART\r\n");
				break;
			default :
				cprintf("Unknown Error Reading Game, error : %d\r\n",ret);
				break;
		}
		
		goto end_function;
	}
	
end_function:
	cmd_busy = 0;
	cmd_ready = 0;
	cmd_size = 0;
	memset(cmd,0,(sizeof(cmd) / sizeof(uint8_t)));
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
		cmd_ready = 1;
		cprintf("\r\n");
		ProcessCommand();
		return;
	}
	else if( 
		(cmd_ready == 0) && 
		((cmd_size+1) < ((sizeof(cmd) / sizeof(uint8_t))-1))
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
	
	cprintf("Ready to rock and roll!\r\n");
    // main loop
	// do not kill the loop. despite the console/UART being set as interrupt. going out of main kills the program completely
    while(1) 
	{
		if((PIND & 0b01000000)==0)
		{
			
		}
    }
}