/*
GB_Cart - An AVR library to communicate with a GB/C cartridge. accessing ROM & RAM
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
#include "gbc_error.h"
#include "GB_Cart.h"
#include "serial.h"

#ifdef GPIO_EXTENDER_MODE
#include "mcp23008.h"
#endif

#define SetPin(x,y) _setPin(&x,(1<<y))
#define ClearPin(x,y) _clearPin(&x,(1<<y))

//the Barebone functions
//--------------------------------
void _setPin(volatile uint8_t *port,uint8_t mask)
{
	*port |= mask;
}
void _clearPin(volatile uint8_t *port,uint8_t mask)
{
	*port &= ~mask;
}
void SetControlPin(uint8_t Pin,uint8_t state)
{
	if(state > 0)
	{
		SetPin(CTRL_PORT,Pin);//CTRL_PORT |= (1 << Pin); // pin goes high
	}	
	else
	{
		ClearPin(CTRL_PORT,Pin); //CTRL_PORT &= ~(1 << Pin); // Pin goes low
	}
	_delay_us(10);
}
int8_t CheckControlPin(uint8_t Pin)
{
	if((CTRL_PIN & (1<< Pin))==0)
		return LOW;
	else
		return HIGH;
}
void ChangeDataPinsMode(uint8_t mode)
{
	if(mode == 0)
	{
		//set as input;
		DATA_DDR &= ~(0xFF); //0b00000000;
		//enable pull up resistors
		DATA_PORT = 0xFF;
	}
	else
	{
		//set as output
		DATA_DDR |= (0xFF);//0b11111111;
		//set output as 0x00
		DATA_PORT = 0x00;
	}

}
void SetupPins(void)
{
	//disable jtag in software, this gives us all the C pins we need.
	/*MCUCSR = (1<<JTD);
    MCUCSR = (1<<JTD);*/
	
	//disable SPI
	/*SPCR &= ~(1 << 6);
	SPCR = 0;*/
	
#ifdef GPIO_EXTENDER_MODE
	
	//setup i2c, as its the source of everything xD
	mcp23008_init(ADDR_CHIP_1);
	mcp23008_init(ADDR_CHIP_2);
	//i2c_Init();
	
	//setup data pins as input
	//DATA_DDR = 0b00000000;
	ChangeDataPinsMode(INPUT); 
	
#elif defined(SHIFTING_MODE)

	//setup data pins as input
	//DATA_DDR = 0b00000000;
	ChangeDataPinsMode(INPUT);
	
	//set the pins as output, init-ing the pins for the shift register
	ADDR_CTRL_DDR |= ( (1 << ADDR_CTRL_DATA) | (1 << ADDR_CTRL_CLK) | (1 << ADDR_CTRL_LATCH) );

#else //elif defined(NORMAL_MODE)
	
	//setup data pins as input
	//DATA_DDR = 0b00000000;
	ChangeDataPinsMode(INPUT); 
	
	//enable address pins A0 - A7 as output
	ADDR_DDR1 = 0xFF;
	ADDR_DDR2 = 0xFF;
	
#endif	

	//disable the external interrupt flags. just to be sure we get those pins correctly. had some issues before, dont know why
	/*GICR &= ~(1 << INT0);
	GICR &= ~(1 << INT1);
#ifdef __ATMEGA32__
	GICR &= ~(1 << INT2);
#endif*/
	
	//setup D pins as well for the other, as output
	CTRL_DDR |= ( (0 << BTN) | (1 << RD) | (1 << WD) | (1 << SRAM) | (1 << RST) );//0b01111100;
	//FUCK TRISTATE BULLSHIT xD set the mode of the pins correctly!
	CTRL_PORT |= ((1 << BTN) | (1 << RD) | (1 << WD) | (1 << SRAM) | (1 << RST) ); //0b01111100;
	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
	SetControlPin(RST,LOW);

}
void SetAddress(uint16_t address)
{
	//check if rst is set high. if not, set it high
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
			
#ifdef GPIO_EXTENDER_MODE
	uint8_t adr1 = address >> 8;
	uint8_t adr2 = (uint8_t)(address & 0xFF);
	
	mcp23008_WriteReg(ADDR_CHIP_1,IODIR,0x00);
	mcp23008_WriteReg(ADDR_CHIP_2,IODIR,0x00);
	
	mcp23008_WriteReg(ADDR_CHIP_1,GPIO,adr1);
	mcp23008_WriteReg(ADDR_CHIP_2,GPIO,adr2);

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
	uint8_t adr1 = address >> 8;
	uint8_t adr2 = (uint8_t)(address & 0xFF);
	
	ADDR_PORT1 = adr1;
	ADDR_PORT2 = adr2;
#endif

	_delay_us(5);
}
uint8_t _ReadByte(int8_t ReadRom, uint16_t address)
{
	uint8_t data = 0;

	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
		
	//pass Address to cartridge via the address bus
	SetAddress(address);
	
	//set Sram control pin low. we do this -AFTER- address is set because MBC2 latches to the address as soon as SRAM is set low. 
	//making the SetAddress do nothing, it'll latch to the address it was set before the SetAddress
	if(ReadRom <= 0)
	{
		SetControlPin(SRAM,LOW);
	}
	
	//set cartridge in read mode
	SetControlPin(RD,LOW);	

	data = DATA_PIN;
	
	SetControlPin(RD,HIGH);
	
	if(ReadRom <= 0)
	{
		SetControlPin(SRAM,HIGH);
	}
	
	return data;
}
uint8_t ReadByte(uint16_t address)
{
	return _ReadByte(1,address);
}
uint8_t ReadRAMByte(uint16_t address)
{
	uint8_t Bank_Type = GameInfo.MBCType;
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	uint8_t ret = _ReadByte(0,address);
	
	if(Bank_Type == MBC2)
	{
		//MBC2 only has the lower 4 bits as data, so we return it as 0xFx
		return (0xF0 | ret );
	}
	else
	{
		return ret;
	}
	
}
uint8_t _WriteByte(int8_t WriteRom, uint16_t addr,uint8_t byte)
{
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	
	ChangeDataPinsMode(OUTPUT);
	DATA_PORT = byte;
	SetAddress(addr);
	
	//set Sram control pin low. we do this -AFTER- address is set because MBC2 latches to the address as soon as SRAM is set low. 
	//making the SetAddress do nothing, it'll latch to the address it was set before the SetAddress
	if(WriteRom <= 0)
	{
		SetControlPin(SRAM,LOW);
	}
	
	SetControlPin(WD,LOW);
	//SetControlPin has a delay
	SetControlPin(WD,HIGH);
	if(WriteRom <= 0)
	{
		SetControlPin(SRAM,HIGH);
	}
	
	DATA_PORT = 0x00;
	ChangeDataPinsMode(INPUT);
	
	return 1;
}
uint8_t WriteByte(uint16_t addr,uint8_t byte)
{	
	return _WriteByte(1,addr,byte);
}
uint8_t WriteRAMByte(uint16_t addr,uint8_t byte)
{

	if(GameInfo.MBCType == MBC_NONE || GameInfo.MBCType == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
		
	return _WriteByte(0,addr,byte);
}
int8_t OpenRam(void)
{
	uint8_t Bank_Type = GameInfo.MBCType;
	
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);	
	
	if(Bank_Type == MBC2)
	{
		//the ghost read fix from https://www.insidegadgets.com/2011/03/28/gbcartread-arduino-based-gameboy-cart-reader-%E2%80%93-part-2-read-the-ram/
		//he said it otherwise had issues with MBC2 :/
		ReadByte(0x0134);
	}
		
	//set banking mode to RAM
	if(Bank_Type == MBC1)
		WriteByte(0x6000,0x01);
	
	//Init the MBC Ram!
	uint16_t init_addr = 0x0000; 
	WriteByte(init_addr,0x0A);
	_delay_us(20);
	
	return 1;
}
int8_t CloseRam(void)
{	
	uint16_t init_addr = 0x0000;	
	
	uint8_t Bank_Type = GameInfo.MBCType;
	//disable RAM again - VERY IMPORTANT -
	if(Bank_Type == MBC1)
		WriteByte(0x6000,0x00);
		
	WriteByte(init_addr,0x00);
	_delay_us(20);

	return 1;
}
void SwitchROMBank(int8_t bank)
{	
	uint8_t Bank_Type = GameInfo.MBCType;
	
	if(Bank_Type == MBC_NONE)
		return;

	//first some preperations
	uint16_t addr = 0x2100;
	uint16_t addr2 = 0x4000;
	 
	if(Bank_Type == MBC1)
	{
		addr = 0x2000;
	}
	if(Bank_Type == MBC5)
	{
		addr2 = 0x3000;
	}
	
	switch(Bank_Type)
	{
		case MBC1:
			//in MBC1 we need to 
			// - set 0x6000 in rom mode 
			WriteByte(0x6000,0);
			WriteByte(addr,bank & 0x1F);
			WriteByte(addr2,bank >> 5);
			break;
		case MBC2:
			WriteByte(addr,bank & 0x1F);
			break;
		case MBC5:
			WriteByte(addr,bank & 0xFF);
			WriteByte(addr2,bank >> 8);
			break;
		default:
		case MBC3:
			WriteByte(addr,bank);
			break;
	}
	
	_delay_us(1);
	
	return;
}
void SwitchRAMBank(int8_t bank)
{
	WriteByte(0x4000,bank);	
	return;
}

//ye idk functions xD
//--------------------------

/*
options : 
	0 : Get Full Header
	1 : Get Name,Rom/Ram Size, Cart Type
*/
int8_t GetHeader(int8_t option)
{
#ifdef SAVE_SPACE
	//this saves alot of space, but at the cost of possible speed since we are forcing it to read everything
	option = 0;
#endif
	CartInfo temp;
	uint8_t header[0x51] = {0};
	uint8_t FF_cnt = 0;
	//memset(header,0,0x51);	
	
	if(option == 0)
	{
		for(uint8_t i = 0 ; i < ((sizeof(header) / sizeof(uint8_t))-1);i++)
		{
			header[i] = ReadByte(0x100+i);
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
	}
	else
	{
		//read logo
		for(uint8_t i = 0 ; i < 0x30;i++)
		{
			header[_CALC_ADDR(_ADDR_LOGO+i)] = ReadByte(0x104+i);
			if(i >= 0 && i<=4 && header[_CALC_ADDR(_ADDR_LOGO+i)] == 0xFF)
			{
				FF_cnt++;
			}
			if(FF_cnt >= 3)
			{
				//data is just returning 0xFF (which is thx to the internal pullups). so no cart!
				SetControlPin(RST,LOW);
				return ERR_FAULT_CART;
			}
		}
		//read old licenseecode
		header[_CALC_ADDR(_ADDR_OLD_LICODE)] = ReadByte(_ADDR_OLD_LICODE);
		//read name
		for(uint8_t i = 0;i <= 16;i++)
		{
			header[_CALC_ADDR(_ADDR_NAME+i)] = ReadByte(_ADDR_NAME+i);
		}
		//read Cart Type
		header[_CALC_ADDR(_ADDR_CART_TYPE)] = ReadByte(_ADDR_CART_TYPE);
		//read rom&ram size
		header[_CALC_ADDR(_ADDR_ROM_SIZE)] = ReadByte(_ADDR_ROM_SIZE);
		header[_CALC_ADDR(_ADDR_RAM_SIZE)] = ReadByte(_ADDR_RAM_SIZE);
	}
	
	//read and compare the logo. this should give it a quick check if its ok or not
	{
		uint8_t Logo[0x30] = {
			0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
			0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
			0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
		};
		uint8_t ReadLogo[0x30];
		
		for(int8_t i = 0;i<0x30;i++)
		{
			ReadLogo[i] = header[_CALC_ADDR(_ADDR_LOGO+i)];
		}
		
		//compare!	
		for(int8_t i =0;i< 0x30;i++)
		{
			//cprintf("comparing %d : %X vs %X\r\n",i,ReadLogo[i],Logo[i]);
			/*cprintf_char(ReadLogo[i]);
			cprintf_char(Logo[i]);*/
			if(ReadLogo[i] == Logo[i])
				continue;
			else
			{
				SetControlPin(RST,LOW);
				return ERR_LOGO_CHECK;
			}
		}
	}
	
	uint8_t NameSize = 16;
	
	temp.OldLicenseeCode = header[_CALC_ADDR(_ADDR_OLD_LICODE)];
	//if its 0x33, we have a cart from past the SGB/GBC era. uses different mapping
	if(temp.OldLicenseeCode == 0x33)
	{
		//read the manufacturer code
		if(option == 0)
		{
			for(uint8_t i = 0;i < 4;i++)
			{
				temp.ManufacturerCode[i] = header[_CALC_ADDR(_ADDR_MANUFACT+i)];
			}
			
			for(uint8_t i = 0;i < 2;i++)
			{
				temp.NewLicenseeCode[i] = header[_CALC_ADDR(_ADDR_NEW_LICODE+i)];
			}	
		}			
		temp.GBCFlag = header[_CALC_ADDR(_ADDR_GBC_FLAG)];
		NameSize = 11;
	}
	else
	{
		//we got an old fashion cart y0
		NameSize = 16;
		temp.GBCFlag = 0x00;
	}

	memset(temp.Name,0,17);
	for(uint8_t i = 0;i < NameSize;i++)
	{
		temp.Name[i] = header[_CALC_ADDR(_ADDR_NAME+i)];
	}
	
	if(option == 0)
	{
		temp.SGBSupported = header[_CALC_ADDR(_ADDR_SGB_SUPPORT)];
		temp.Region = header[_CALC_ADDR(_ADDR_REGION)];
		temp.RomVersion = header[_CALC_ADDR(_ADDR_ROM_VERSION)];
		
		//todo, verify header
		temp.HeaderChecksum = header[_CALC_ADDR(_ADDR_HEADER_CHECKSUM)];
		
		for(uint8_t i = 0;i < 2;i++)
		{
			temp.GlobalChecksum[i] = header[_CALC_ADDR(_ADDR_GLBL_CHECKSUM+i)];
		}
	}
	
	temp.CartType = header[_CALC_ADDR(_ADDR_CART_TYPE)];
	temp.RomSize = header[_CALC_ADDR(_ADDR_ROM_SIZE)];
	temp.RamSize = header[_CALC_ADDR(_ADDR_RAM_SIZE)];	
	temp.MBCType = GetMBCType(temp.CartType);
	
	GameInfo = temp;
	SetControlPin(RST,LOW);
	return 1;
	
}
int8_t GetGameInfo(void)
{
	return GetHeader(1);
}
uint16_t GetRomBanks(uint8_t RomSize)
{
	if(RomSize > 7)
	{
		switch(RomSize)
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
	else
	{
		return 2 << GameInfo.RomSize;
	}
}
int8_t GetRamDetails(uint16_t *end_addr, uint8_t *banks,uint8_t RamSize)
{
	if(end_addr == NULL || banks == NULL)
		return ERR_MBC_SAVE_UNSUPPORTED;
	
	uint8_t Bank_Type = GameInfo.MBCType;
	
	if(Bank_Type == MBC2)
	{		
		//Set the Ram Size & end addr. MBC2 is euh...special :P
		*banks = 1;
		*end_addr = 0xA200;
	}
	else
	{
		*end_addr = 0xC000;
		switch(RamSize)
		{
			case 0x01: //1 bank of 2KB ram
				*banks = 1;
				*end_addr = 0xA800;
				break;
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

//User End Functions
//--------------------------
#ifndef DISABLE_CORE_DUMP_FUNCTIONS
int8_t DumpROM()
{
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);	
	
	//reset game cart. this causes all banks & states to reset
	SetControlPin(RST,LOW);
	_delay_us(50);
	SetControlPin(RST,HIGH);
	
	if(GameInfo.Name[0] == 0x00)
	{
		//header isn't loaded yet!
		int8_t ret = GetHeader(1);
		if(ret <= 0)
		{
			return ret;
		}
	}
	
	uint16_t banks = GetRomBanks(GameInfo.RomSize);
	
	for(uint16_t bank = 1;bank < banks;bank++)
	{
		uint16_t addr = 0x4000;
		if(bank <= 1)
		{
			addr = 0x00;
		}
		else if(bank > 1)
		{
			SwitchROMBank(bank);//GetMBCType(GameInfo.CartType));
		}
		for(;addr < 0x8000;addr++)
		{
			cprintf_char(ReadByte(addr));
		}
	}
	SetControlPin(RST,LOW);
	return 1;
}
int8_t DumpRAM()
{	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
	
	//reset game cart. this causes all banks & states to reset
	SetControlPin(RST,LOW);
	_delay_us(50);
	SetControlPin(RST,HIGH);
	
	if(GameInfo.Name[0] == 0x00)
	{
		//header isn't loaded yet!
		int8_t ret = GetHeader(1);
		if(ret <= 0)
		{
			return ret;
		}
	}
	
	uint8_t Bank_Type = GameInfo.MBCType;//GetMBCType(GameInfo.CartType);
	
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
		
	if(Bank_Type != MBC2 && GameInfo.RamSize <= 0)
		return ERR_NO_SAVE;

	
	//read RAM Address'
	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000; //actually ends at 0xBFFF
	uint8_t banks = 0;
	
	int8_t ret = GetRamDetails(&end_addr,&banks,GameInfo.RamSize);
	
	if(ret < 0)
		return ret;
		
	OpenRam();

	for(uint8_t bank = 0;bank < banks;bank++)
	{
		//if we aren't dealing with MBC2, set bank to 0!
		if(Bank_Type != MBC2)
			SwitchRAMBank(bank);
			
		for(uint16_t i = addr;i< end_addr ;i++)
		{
			cprintf_char(ReadRAMByte(i));
			_delay_us(20);
		}
	}
	
	CloseRam();
	SetControlPin(RST,LOW);
	
	return 1;
}
int8_t WriteRAM()
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
		ret = GetHeader(1);
		if(ret <= 0)
		{	
			goto end_function;
		}
	}

	uint16_t addr = 0xA000;
	uint16_t end_addr = 0xC000;
	uint8_t banks = 0;
	uint8_t Bank_Type = GameInfo.MBCType;//GetMBCType(GameInfo.CartType);
	
	ret = GetRamDetails(&end_addr,&banks,GameInfo.RamSize);
	if(ret < 0)
		goto end_function;
	
	OpenRam();
		
	for(uint8_t bank = 0;bank < banks;bank++)
	{
		//end_addr = 0xA200;
		//uint8_t bank = 0;
		//if we aren't dealing with MBC2, set bank to 0!
		if(Bank_Type != MBC2)
			SwitchRAMBank(bank);		
		
		uint8_t data_recv;
		
		for(uint16_t i = addr;i< end_addr;i++)
		{					
			// Wait for incoming data
			while ( !(UCSRA & (_BV(RXC))) );	
			//add the delay because the while tends to exit once in a while to early and it makes us retrieve the wrong byte. for example 0xFA often tended to become 00
			//a 5ms delay seemed to cause it different times? :/
			_delay_ms(1);
			// Return the data
			data_recv = UDR;
			
			WriteRAMByte(i,data_recv);	
			uint8_t data = ReadRAMByte(i);
			cprintf_char(data);
			
		}
	}
	
	CloseRam();
end_function:
	SetControlPin(RST,LOW);
	//re-enable interrupts!
	sei();
	return ret;
}
#endif