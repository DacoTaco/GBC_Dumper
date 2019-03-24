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

/*GBA cart pins are as following : 	
	
    1 - VCC – Power (5 volts)
    2 - CLK – Clock signal (not used)
    3 - WR – if low(grounded) and if RD is low, we can write to the SRAM and load a ROM or SRAM bank
    4 - RD – if low (grounded) and if WR is high, we can read the ROM and SRAM
    5 - CS_ROM – if low, selects the ROM
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
	21 - A15 - the 16 addresses lines that we tell the ROM which particular byte of data we want to read, also the 16 bit data read from that location
    22 - A16/D0
	23 - A17/D1
	24 - A18/D2
	25 - A19/D3
	26 - A20/D4
	27 - A21/D5
	28 - A22/D6
	29 - A23/D7 - 8 higher bits of the address of the ROM or 8 bit output when reading RAM
    30 - CS_SRAM - if low, selects the RAM
    31 - Audio in - (not used)
    32 - GND - Ground	
*/
/*
Connections we want,need and assigned to :

    VCC -> 2nd VCC pin/Input voltage (5v)
    ~WR -> D3 (has 10k pulldown)
    ~RD -> D2 (has 10k pulldown)
    CS_ROM -> D4 (has 10k pulldown)
    A0 - A15 -> B0 - B7 & C0 - C7
    D0 - D7 -> A0 - A7
    CS_SRAM -> D5 (has 10k resistor in between) and led to indicate if its ready or not
    GND -> input/Second GND

*/

#define ReadGBABytes(x) _ReadGBABytes(1,x)

uint16_t _ReadGBABytes(uint8_t ReadRom,uint32_t address);
void SetGBAAddress(uint32_t address);
void SetGBADataAsOutput(void);
void SetGBADataAsInput(void);

