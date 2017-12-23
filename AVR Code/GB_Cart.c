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
#include "GB_Cart.h"
#include "serial.h"

//the Barebone functions
//--------------------------------
int8_t CheckControlPin(uint8_t Pin)
{
	if((CTRL_PIN & (1<< Pin))==0)
		return LOW;
	else
		return HIGH;
}
void SetControlPin(uint8_t Pin,uint8_t state)
{
	if(state > 0)
	{
		CTRL_PORT |= (1 << Pin); // pin goes high
	}	
	else
	{
		CTRL_PORT &= ~(1 << Pin); // Pin goes low
	}
}
void ChangeDataPinsMode(uint8_t mode)
{
	if(mode == 0)
	{
		//set as input;
		DDRA &= ~(0xFF); //0b00000000;
	}
	else
	{
		//set as output
		DDRA |= (0xFF);//0b11111111;
	}

}
void SetupPins(void)
{
	//disable jtag in software, this gives us all the C pins we need.
	/*MCUCSR = (1<<JTD);
    MCUCSR = (1<<JTD);*/
	
	//disable SPI
	//SPCR &= ~(1 << 6);
	//SPCR = 0;
	
	//setup data pins as input
	//DDRA = 0b00000000;
	ChangeDataPinsMode(INPUT);
	
	//enable address pins A0 - A7 as output
	DDRB = 0xFF;
	DDRC = 0xFF;
	
	//disable the external interrupt flags. just to be sure we get those pins correctly. had some issues before, dont know why
	/*GICR &= ~(1 << INT0);
	GICR &= ~(1 << INT1);
	GICR &= ~(1 << INT2);*/
	
	//setup D pins as well for the other, as output
	DDRD |= 0b01111100;
	//FUCK TRISTATE BULLSHIT xD set the mode of the pins correctly!
	PORTD |= 0b01111100;
	
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
		
	uint8_t adr1 = address >> 8;
	uint8_t adr2 = (uint8_t)(address & 0xFF);
	//cprintf("setting address to 0x%02X%02X\r\n",adr1,adr2);
	
	PORTB = adr1;
	PORTC = adr2;
}
uint8_t GetByte(uint16_t address)
{
	uint8_t data = 0;

	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	//check if rst is set high. if not, set it high
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
		
	//pass Address to cartridge via the address bus
	SetAddress(address);

	//set cartridge in read mode
	SetControlPin(RD,LOW);	
	_delay_us(1);
	
	data = PINA;
	
	SetControlPin(RD,HIGH);
	
	return data;
}
uint8_t GetRAMByte(uint16_t address,uint8_t Bank_Type)
{
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	
	SetControlPin(SRAM,LOW);
	uint8_t ret = GetByte(address);
	SetControlPin(SRAM,HIGH);
	
	if(Bank_Type == MBC2)
	{
		//MBC2 only has the lower 4 bits as data, so we return it as 0xFx
		return 0xF0 | (ret & 0x0F);
	}
	else
	{
		return ret;
	}
}
uint8_t WriteByte(uint16_t addr,uint8_t byte)
{	
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	
	ChangeDataPinsMode(OUTPUT);
	PORTA = byte;
	SetAddress(addr);
	
	SetControlPin(WD,LOW);
	_delay_us(3);
	
	SetControlPin(WD,HIGH);
	_delay_us(1);
	PORTA = 0x00;
	ChangeDataPinsMode(INPUT);
	
	return 1;
}
uint8_t WriteRAMByte(uint16_t addr,uint8_t byte,uint8_t Bank_Type)
{

	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
		
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);
		
	SetControlPin(SRAM,LOW);
	uint8_t ret = WriteByte(addr,byte);
	_delay_us(2);
	SetControlPin(SRAM,HIGH);

	return ret;

}
int8_t OpenRam(void)
{
	if(GameInfo.Name[0] == 0x00)
	{
		//header isn't loaded yet!
		int8_t ret = GetGameInfo();
		if(ret <= 0)
		{
			return ret;
		}
	}

	uint8_t Bank_Type = GetMBCType(GameInfo.CartType);
	
	if(Bank_Type == MBC_NONE || Bank_Type == MBC_UNSUPPORTED)
		return ERR_NO_MBC;
	
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	SetControlPin(WD,HIGH);
	SetControlPin(RD,HIGH);
	SetControlPin(SRAM,HIGH);	
	
	if(Bank_Type == MBC2)
	{
		//the ghost read fix from https://www.insidegadgets.com/2011/03/28/gbcartread-arduino-based-gameboy-cart-reader-%E2%80%93-part-2-read-the-ram/
		//he said it otherwise had issues with MBC2 :/
		GetByte(0x0134);
	}
		
	//set banking mode to RAM
	if(Bank_Type == MBC1)
		WriteByte(0x6000,0x01);
	
	//Init the MBC Ram!
	uint16_t init_addr = 0x0000; 
	WriteByte(init_addr,0x0A);
	_delay_us(2);
	
	return 1;
}
int8_t CloseRam(void)
{
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	
	uint16_t init_addr = 0x0000;
	
	if(GameInfo.Name[0] == 0x00)
	{
		//header isn't loaded yet!
		int8_t ret = GetGameInfo();
		if(ret <= 0)
		{
			return ret;
		}
	}
	
	
	
	uint8_t Bank_Type = GetMBCType(GameInfo.CartType);
	//disable RAM again - VERY IMPORTANT -
	if(Bank_Type == MBC1)
		WriteByte(0x6000,0x00);
		
	WriteByte(init_addr,0x00);
	_delay_us(2);

	return 1;
}
void SwitchROMBank(int8_t bank,uint8_t Bank_Type)
{	
	if(Bank_Type == MBC_NONE)
		return;
		
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}

	//first some preperations
	uint16_t addr = 0x2100;
	uint16_t addr2 = 0x4000;
	 
	if(Bank_Type == MBC1)
	{
		addr = 0x2000;
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
			WriteByte(addr,bank & 0x7F);
			WriteByte(addr2,bank >> 7);
			break;
		default:
		case MBC3:
			WriteByte(addr,bank);
			break;
	}
	
	_delay_ms(5);
	
	return;
}
void SwitchRAMBank(int8_t bank,uint8_t Bank_Type)
{
	if(CheckControlPin(RST) == 0)
	{
		SetControlPin(RST,HIGH);
	}
	
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

	CartInfo temp;
	uint8_t header[0x51];
	memset(header,0,0x51);	
	
	if(option == 0)
	{
		for(uint8_t i = 0 ; i < ((sizeof(header) / sizeof(uint8_t))-1);i++)
		{
			header[i] = GetByte(0x100+i);
		}	
	}
	else
	{
		//read logo
		for(uint8_t i = 0 ; i < 0x30;i++)
		{
			header[_CALC_ADDR(_ADDR_LOGO+i)] = GetByte(0x104+i);
		}
		//read old licenseecode
		header[_CALC_ADDR(_ADDR_OLD_LICODE)] = GetByte(_ADDR_OLD_LICODE);
		//read name
		for(uint8_t i = 0;i <= 16;i++)
		{
			header[_CALC_ADDR(_ADDR_NAME+i)] = GetByte(_ADDR_NAME+i);
		}
		//read Cart Type
		header[_CALC_ADDR(_ADDR_CART_TYPE)] = GetByte(_ADDR_CART_TYPE);
		//read rom&ram size
		header[_CALC_ADDR(_ADDR_ROM_SIZE)] = GetByte(_ADDR_ROM_SIZE);
		header[_CALC_ADDR(_ADDR_RAM_SIZE)] = GetByte(_ADDR_RAM_SIZE);
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
			if(ReadLogo[i] == Logo[i])
				continue;
			else
			{
				return ERR_FAULT_CART;
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
			
			temp.GBCFlag = header[_CALC_ADDR(_ADDR_GBC_FLAG)];
			
			for(uint8_t i = 0;i < 2;i++)
			{
				temp.NewLicenseeCode[i] = header[_CALC_ADDR(_ADDR_NEW_LICODE+i)];
			}	
		}				
		NameSize = 11;
	}
	else
	{
		//we got an old fanshion cart y0
		NameSize = 16;
	}

	memset(temp.Name,0,17);
	for(uint8_t i = 0;i <= NameSize;i++)
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
	
	GameInfo = temp;
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
int8_t GetRamDetails(uint8_t Bank_Type, uint16_t *end_addr, uint8_t *banks,uint8_t RamSize)
{
	if(end_addr == NULL || banks == NULL)
		return ERR_MBC_SAVE_UNSUPPORTED;
		
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
		case 0xFF: //HuC1 + RAM + BATTERY
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
			SwitchROMBank(bank,GetMBCType(GameInfo.CartType));
		}
		for(;addr < 0x8000;addr++)
		{
			cprintf_char(GetByte(addr));
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
	
	uint8_t Bank_Type = GetMBCType(GameInfo.CartType);
	
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
			//cprintf("byte 0x%02X : 0x%02X\r\n",i-addr,GetRAMByte(i,Bank_Type));
			cprintf_char(GetRAMByte(i,Bank_Type));
			_delay_us(2);
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
	uint8_t Bank_Type = GetMBCType(GameInfo.CartType);
	
	ret = GetRamDetails(Bank_Type,&end_addr,&banks,GameInfo.RamSize);
	if(ret < 0)
		goto end_function;
	
	OpenRam();
		
	for(uint8_t bank = 0;bank < banks;bank++)
	{
		//end_addr = 0xA200;
		//uint8_t bank = 0;
		//if we aren't dealing with MBC2, set bank to 0!
		if(Bank_Type != MBC2)
			SwitchRAMBank(bank,Bank_Type);		
		
		uint8_t data_recv;
		
		//switch bank!
		if(Bank_Type != MBC2)
			SwitchRAMBank(bank,Bank_Type);
		
		for(uint16_t i = addr;i< end_addr;i++)
		{					
			// Wait for incoming data
			while ( !(UCSRA & (_BV(RXC))) );	
			//add the delay because the while tends to exit once in a while to early and it makes us retrieve the wrong byte. for example 0xFA often tended to become 00
			//a 5s delay seemed to cause it different times? :/
			_delay_ms(1);
			// Return the data
			data_recv = UDR;
			
			WriteRAMByte(i,data_recv,Bank_Type);	
			uint8_t data = GetRAMByte(i,Bank_Type);
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