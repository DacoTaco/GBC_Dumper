/*
GBA_Cart - An AVR library to communicate with a GBA cartridge. accessing ROM & RAM
Copyright (C) 2016-2019  DacoTaco
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
#include "gbc_error.h"
#include "GBA_Cart.h"
#include "GB_Cart.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif

/*
(10:14:36 PM) DacoTaco: nitro2k01: from what i understand so far of GBA is that CS makes you access rom and CS2 access ram? (and does it latch the address when those are pulled high or when RD/WD are pulled low, like on GB/C ? )
(10:16:36 PM) nitro2k01: I believe CS2 works like a "traditional" 16 bit CS.When CS and RD are both low for example, a read is triggered.
(10:17:12 PM) nitro2k01: CS I believe latches the address on a falling edge, then you hold it low and do repeated reads or writes.
(10:17:58 PM) nitro2k01: On every negative pulse of RD or WR, a read or write is triggered. On every positive pulse of either of those, the latched value is incremented.
(10:18:47 PM) nitro2k01: So sequential reads don't stall the bus by requiring latching the value again on each increment.
(10:19:58 PM) nitro2k01: Note whoever that CS2 is on a different pin than CS would be on the older GB connector.
(10:22:42 PM) DacoTaco: CS2 = rom pin ? the image i have here says CS2 is pin 30, aka reset/CS for GB
(10:22:52 PM) DacoTaco: also, nice to know it has auto increment. makes dumping easier :P
(10:23:43 PM) nitro2k01: Just reset for GB.
(10:24:16 PM) nitro2k01: However, it's often unconnected so don't think you can keep the cart in reset and prevent reads and detect a GBA cart that way.
(10:29:46 PM) DacoTaco: im properly using the reset pin so far anyway. can't have it unconnected. but i guess ill have to incorporate a 'switch' in it somehow. be it a software/transistor switch or a physical switch. maybe ill think of behaviour setup/detection along the way
(10:30:57 PM) nitro2k01: No, I was saying many *cartridges* have it unconnected, so you can't rely on a GB cart being held in reset, do distinguish a GB cart from a GBA cart.
(10:32:16 PM) DacoTaco: what, reset unconnected? i thought that pin was actually used by every game
*/

inline void SetGBADataAsOutput(void)
{
	//disable pull ups
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0x00);
	
	//set output
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0x00);
	
}
inline void SetGBADataAsInput(void)
{
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0xFF);
	
	//enable pull up
	mcp23008_WriteReg(ADDR_CHIP_1,GPPU,0xFF);
	mcp23008_WriteReg(ADDR_CHIP_2,GPPU,0xFF);
}
inline void SetGBAAddress(uint32_t address)
{	
#ifdef GPIO_EXTENDER_MODE
	SetGBADataAsOutput();
	SetDataPinsAsOutput();
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,(uint8_t)(address & 0xFF));
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,(uint8_t)(address >> 8) & 0xFF);
	mcp23008_WriteReg(DATA_CHIP_1,GPIO,(uint8_t)(address >> 16) & 0xFF);
#endif
}
inline uint16_t _ReadGBABytes(uint8_t ReadRom,uint32_t address)
{
	uint8_t d1 = 0;
	uint8_t d2 = 0;

	SetPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);

	SetGBAAddress(address);
	
	//latch the address
	ClearPin(CTRL_PORT,CS1);
	
	//set RD low so data pins are set
	ClearPin(CTRL_PORT,RD);

#ifdef GPIO_EXTENDER_MODE	
	//set as input so we can read the pins
	SetGBADataAsInput();
#endif
	
#ifdef GPIO_EXTENDER_MODE			
	//read data
	mcp23008_ReadReg(ADDR_CHIP_1, GPIO,&d1);
	mcp23008_ReadReg(ADDR_CHIP_2, GPIO,&d2);	
#endif
	
	SetPin(CTRL_PORT,CS2);
	SetPin(CTRL_PORT,RD);
	SetPin(CTRL_PORT,CS1);
	
	return (uint16_t)d2 << 8 | d1;
}