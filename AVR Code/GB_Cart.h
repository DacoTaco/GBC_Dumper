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
/*
Connections we want,need and assigned to :

    VCC -> 2nd VCC pin/Input voltage (5v)
    ~WR –> D3 (has 10k pulldown)
    ~RD –> D2 (has 10k pulldown)
    CS_SRAM –> D4 (has 10k pulldown)
    A0 – A15 –> B0 - B7 & C0 - C7
    D0 – D7 –> A0 - A7
    Reset –> D5 (has 10k resistor in between) and led to indicate if its busy or not
    GND -> input/Second GND

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

//define all pins for a specific chip!
#ifdef GPIO_EXTENDER_MODE	
	
	#define GET_DATA(x) { mcp23008_ReadReg(DATA_CHIP_1, GPIO,&x);}
	#define SET_DATA(x) { mcp23008_WriteReg(DATA_CHIP_1, GPIO,x);}
	
	#define CTRL_DDR DDRD
	#define CTRL_PORT PORTD
	#define CTRL_PIN PIND
	
	//the MCP23008 addresses consist of 0b0100xxxy. xxx = the chip set address & y = the read/write bit.
	//but no worries, the mcp23008 library keeps that in check for us in case we fuck up
	#define BASE_ADDR_CHIPS 0b01000000
		
	#define ADDR_CHIP_1 (0b01000000)
	#define ADDR_CHIP_2 (0b01000010)
	#define DATA_CHIP_1 (0b01000100)
	
	
	#define RD PD5
	#define WD PD6
	#define SRAM PD7
	#define RST PD3
	#define BTN PD4
	#define PING_PIN PD5
	
#elif defined(SHIFTING_MODE)

	#define DATA_PORT PORTB
	#define DATA_DDR DDRB
	#define DATA_PIN PINB
	
	#define GET_DATA(x) (x = DATA_PIN)
	#define SET_DATA(x) (DATA_PORT = byte)
	
	#define ADDR_CTRL_DDR DDRC
	#define ADDR_CTRL_PORT PORTC
	#define ADDR_CTRL_PIN PINC
	
	#define ADDR_CTRL_DATA PC2
	#define ADDR_CTRL_CLK PC3
	#define ADDR_CTRL_LATCH PC4
	
	#define CTRL_DDR DDRD
	#define CTRL_PORT PORTD
	#define CTRL_PIN PIND
	
	#define RD PD2
	//0b00100000;
	#define WD PD3
	//0b01000000;
	#define SRAM PD4
	//0b10000000;
	#define RST PD5
	//0b00000100;
	#define BTN PD6
	//0b00100000
	
#else
		
	#define DATA_PORT PORTA
	#define DATA_DDR DDRA
	#define DATA_PIN PINA
	
	#define GET_DATA(x) (x = DATA_PIN)
	#define SET_DATA(x) (DATA_PORT = byte)
	
	#define ADDR_DDR1 DDRB
	#define ADDR_DDR2 DDRC
	#define ADDR_PORT1 PORTB
	#define ADDR_PORT2 PORTC

	
	#define CTRL_DDR DDRD
	#define CTRL_PORT PORTD
	#define CTRL_PIN PIND
	#define RD PD2
	//0b00000100;
	#define WD PD3
	//0b00001000;
	#define SRAM PD4
	//0b00010000;
	#define RST PD5
	//0b00100000;
	#define BTN PD6
	//0b01000000
	
#endif

#define HIGH 1
#define LOW 0

#define INPUT 0
#define OUTPUT 1

typedef struct _cartInfo
{
	char Name[17]; // 0x134 - 0x143
	uint8_t ManufacturerCode[4]; //0x13F - 0x142
	uint8_t GBCFlag; // 0x143
	uint8_t NewLicenseeCode[2]; // 0x144 - 0x145
	uint8_t SGBSupported; //0x146
	uint8_t CartType; //0x147 this is what tells us what MBC and save etc it has
	uint8_t MBCType; //byte we use to store the MBC Type
	uint16_t RomSize; //0x148 , if(Romsize > 0 && Romsize < 8 ) - > for(romsize) -> 32 * 2 else 32 
	uint16_t RamSize; // 0x149 0 = 0, 1 = 2 , 2 = 8, 3 = 32 (table of 4)
	uint8_t Region; // 0x14A
	uint8_t OldLicenseeCode; // 0x14B, if new is used -> 0x33
	uint8_t RomVersion; // 0x14C
	uint8_t HeaderChecksum; // 0x14D
	uint8_t GlobalChecksum[2]; // 0x14E - 0x14F
} CartInfo ;

CartInfo GameInfo;


//--------------
//functions
//--------------

//main functions. these are the most used by other code
void SetupPins(void);

//other , usable functions by other code. these are used by the GB/C Cart code !
void SetControlPin(uint8_t Pin,uint8_t state);
void SetAddress(uint16_t address);

#define SetPin(x,y) x |= (1<<y)
#define ClearPin(x,y) x &= ~(1<<y)
#define ReadByte(x) _ReadByte(1,x)
#define WriteByte(x,y) _WriteByte(1,x,y)
void _setPin(volatile uint8_t *port,uint8_t mask);
void _clearPin(volatile uint8_t *port,uint8_t mask);
uint8_t _ReadByte(int8_t ReadRom, uint16_t address);
void _WriteByte(int8_t writeRom, uint16_t addr,uint8_t byte);


int8_t CheckControlPin(uint8_t Pin);
uint8_t ReadRAMByte(uint16_t address);
int8_t WriteRAMByte(uint16_t addr,uint8_t byte);

int8_t OpenRam(void);
void CloseRam(void);

void SwitchRAMBank(int8_t bank);
void SwitchROMBank(int8_t bank);

uint16_t GetRomBanks(uint8_t RomSize);
int8_t GetRamDetails(uint16_t *end_addr, uint8_t *banks,uint8_t RamSize);
uint8_t GetMBCType(uint8_t CartType);
int8_t GetGameInfo(void);



