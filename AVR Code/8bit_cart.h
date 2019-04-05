/*
8bit_cart - An AVR library to communicate with a GB/C cartridge. accessing ROM & RAM
Copyright (C) 2018-2019  DacoTaco
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

/*GB(C) cart pins are as following : 	
	Taken from https://www.insidegadgets.com/2011/03/19/gbcartread-arduino-based-gameboy-cart-reader-%E2%80%93-part-1-read-the-rom/ || https://www.insidegadgets.com/wp-content/uploads/2011/03/IMG_1994.jpg
    1 - VCC – Power (5 volts)
    2 - CLK – Clock signal (not used)
    3 - WR – if low(grounded) and if RD is low, we can write to the SRAM and load a ROM or SRAM bank
    4 - RD – if low (grounded) and if WR is high, we can read the ROM and SRAM
    5 - CS_SRAM – if enabled, selects the SRAM
    6 - A0
	7 - A1
	8 - A2
	9 - A3
	10 - A4
	11 - A5
	12 - A6
	13 - A7
	14 - A8
	15 - A9
	16 - A10
	17 - A11
	18 - A12
	19 - A13
	20 - A14
	21 - A15 – the 16 addresses lines that we tell the ROM which particular byte of data we want to read
    22 - D0
	23 - D1
	24 - D2
	25 - D3
	26 - D4
	27 - D5
	28 - D6
	29 - D7 – the 8 data lines that we read the byte of data selected by the 16 address lines. These data lines can also be used to control which ROM bank to load
    30 - Reset – set high to set cart active. low to make it inactive. low and then high resets cart
    31 - Audio in – (not used)
    32 - GND – Ground
	
				RD		WD		RESULT
	STATE 		1		1		NOTHING
				0		1		READING
				1		0		CHANGE BANK/Write MBC
				0		0		WRITE SRAM/LOAD RAM?
	
*/

//addresses defines
#define _CALC_ADDR(x) (x-0x100)
#define _ADDR_LOGO 0x104
#define _ADDR_NAME 0x134
#define _ADDR_MANUFACT 0x13F
#define _ADDR_GBC_FLAG 0x143
#define _ADDR_NEW_LICODE 0x144
#define _ADDR_SGB_SUPPORT 0x146
#define _ADDR_CART_TYPE 0x147
#define _ADDR_ROM_SIZE 0x148
#define _ADDR_RAM_SIZE 0x149
#define _ADDR_REGION 0x14A
#define _ADDR_OLD_LICODE 0x14B
#define _ADDR_ROM_VERSION 0x14C
#define _ADDR_HEADER_CHECKSUM 0x014D
#define _ADDR_GLBL_CHECKSUM 0x14E

//MBC TYPES
#define MBC_UNSUPPORTED 0x00
#define MBC_NONE 0x09
#define MBC1 0x10
#define MBC2 0x20
#define MBC3 0x30
#define MBC4 0x40
#define MBC5 0x50

typedef struct _GBC_Header
{
	char Name[17]; // 0x134 - 0x143
	uint8_t ManufacturerCode[4]; //0x13F - 0x142
	uint8_t GBCFlag; // 0x143
	uint8_t NewLicenseeCode[2]; // 0x144 - 0x145
	uint8_t SGBSupported; //0x146
	uint8_t CartType; //0x147 this is what tells us what MBC and save etc it has
	uint8_t MBCType; //byte we use to store the MBC Type
	uint8_t RomSizeFlag; //0x148 , if(Romsize > 0 && Romsize < 8 ) - > for(romsize) -> 32 * 2 else 32 
	uint8_t RamSize; // 0x149 0 = 0, 1 = 2 , 2 = 8, 3 = 32 (table of 4)
	uint8_t Region; // 0x14A
	uint8_t OldLicenseeCode; // 0x14B, if new is used -> 0x33
	uint8_t RomVersion; // 0x14C
	uint8_t HeaderChecksum; // 0x14D
	uint8_t GlobalChecksum[2]; // 0x14E - 0x14F
} GBC_Header ;

uint8_t LoadedBankType;

//-------------------------
//general functions
//-------------------------
void Setup_Pins_8bitMode(void);
void Set8BitAddress(uint16_t address);

//we pass on RD here so it will do nothing to the pins. lost cycles, but i don't know if a null check would be more effecient?
#define ReadGBRomByte(x) _Read8BitByte(0,x)
#define ReadGBARamByte(x) _Read8BitByte(CS2,x)
uint8_t ReadGBRamByte(uint16_t address);
uint8_t _Read8BitByte(uint8_t CS_Pin, uint16_t address);

#define WriteGBRomByte(a,d) _Write8BitByte(0,a,d);
#define WriteGBARamByte(a,d) _Write8BitByte(CS2,a,d);
int8_t WriteGBRamByte(uint16_t addr,uint8_t byte);
void _Write8BitByte(uint8_t CS_Pin, uint16_t addr,uint8_t byte);

//-------------------------
//GB related functions
//-------------------------
int8_t OpenGBRam(void);
void CloseGBRam(void);
void SwitchROMBank(int8_t bank);
void SwitchRAMBank(int8_t bank);
int8_t GetGBInfo(char* GameName, uint8_t* romFlag , uint8_t* ramFlag);
uint16_t GetAmountOfRomBacks(uint8_t RomSizeFlag);
int8_t GetRamDetails(uint16_t *end_addr, uint8_t *banks,uint8_t RamSizeFlag);
uint8_t GetMBCType(uint8_t CartType);



