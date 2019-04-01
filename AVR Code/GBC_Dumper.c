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
#include "gb_error.h"
#include "gbc_api.h"
#include "GB_Cart.h"
#include "GBA_Cart.h"


uint8_t cmd_size = 0;
#define MAX_CMD_SIZE 0x21
char cmd[MAX_CMD_SIZE];
uint8_t process_cmd = 0;

/*

//things that are broken : 

*/

void ProcessCommand(void)
{
	process_cmd = 0;
	DisableSerialInterrupt();
	API_SetupPins((PINC&PC1)==0);
	int8_t ret = API_GetGameInfo();
	
	if(ret > 0)
	{
		if(strncmp(cmd,"API_READ_ROM",API_READ_ROM_SIZE) == 0 || strncmp(cmd,"API_READ_RAM",API_READ_RAM_SIZE) == 0 )
		{
			ROM_TYPE type = (strncmp(cmd,"API_READ_ROM",API_READ_ROM_SIZE) == 0)?TYPE_ROM:TYPE_RAM;
			ret = API_Get_Memory(type);
		}
		else if(strncmp(cmd,"API_WRITE_RAM",API_WRITE_RAM_SIZE) == 0)
		{			
			ret = API_WriteRam();	
		}
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
		//if there is no Gameinfo/Cart detected that means we are here from cart detection or before. therefor send an abort.
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
	memset(cmd,0,MAX_CMD_SIZE);
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
			process_cmd = 1;
		return;
	}
	else if( 
		(cmd_size+1) < MAX_CMD_SIZE
	)
	{
		//space left!
		cmd[cmd_size] = byte;
		cmd_size++;
	}
	return;
}
int main(void)
{
	//fire up the usart
	initConsole();
	
	//setup API
	API_Init();
	
	//Set PD2 as Input
	DDRC &= ~(1 << PC1);
	//disable pull up
	PORTC &= ~(1 << PC1);

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
	uint32_t addr = 0x000000;//0xFF31;//0x13FF;//0x104;//0x200;//0x8421;
    while(1) 
	{
		if(process_cmd)
		{
			ProcessCommand();
		}
		if(CheckControlPin(BTN) == LOW)
		{
			cprintf("Btn pressed!\r\n");
			SetControlPin(CS2,HIGH);
			uint16_t data = 0;	
			//expected : 0x2e00	
			//cprintf("address (0x%X): 0x%02X%02X%02X\r\n",addr, addr & 0xFF,(addr >> 8) & 0xFF,(addr >> 16) & 0xFF);*/
			data = ReadGBABytes(addr);
			uint8_t d1 = data >> 8;
			uint8_t d2 = data & 0xFF;
			
			cprintf("data : ");
			cprintf_char(d1);	
			cprintf_char(d2);
			
			cprintf("\r\ndone\r\n");

			if(CheckControlPin(CS2) == 1)
			{
				SetControlPin(CS2,LOW);
			}
			//addr++;*/
			_delay_ms(200);
		}
    }
}