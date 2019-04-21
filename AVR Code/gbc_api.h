/*
gb_api - AVR Code/API to access the GBA/GBC library to communicate with a GBA/GB(C) cartridge. accessing ROM & RAM
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

#define API_GB_CART_TYPE_START 0x76
#define API_GB_CART_TYPE_END 0x77
#define API_GBC_ONLY 0x78
#define API_GBC_HYBRID 0x79
#define API_GBC_GB 0x7A
#define API_GBA_ONLY 0x7B

#define API_GAMENAME_START 0x86
#define API_GAMENAME_END 0x87
#define API_FILESIZE_START 0x96
#define API_FILESIZE_END 0x97


#define API_OK 0x10
#define API_NOK 0x11
#define API_VERIFY 0x12
#define API_RESET 0x13

#define API_HANDSHAKE_DENY 0x15
#define API_HANDSHAKE_REQUEST 0x017
#define API_HANDSHAKE_ACCEPT 0x06

#define API_TASK_START 0x20
#define API_TASK_FINISHED 0x21
#define API_CART_MODE_AUTO 0x22
#define API_CART_MODE_GB 0x23
#define API_CART_MODE_GBA 0x24

#define API_MODE_READ_ROM 0xD0;
#define API_MODE_READ_RAM 0xD1;
#define API_MODE_WRITE_RAM 0xD2;

#define API_ABORT 0xF0
#define API_ABORT_ERROR 0xF1
#define API_ABORT_CMD 0xF2
#define API_ABORT_PACKET 0xF3

typedef uint8_t ROM_TYPE;
#define TYPE_ROM 0
#define TYPE_RAM 1

//main API functions
void API_Init(void);
void API_SetupPins(int8_t _gb_mode);
int8_t API_CartInserted(void);
int8_t API_GetGameInfo(void);
void API_ResetGameInfo(void);
int8_t API_Get_Memory(ROM_TYPE type,int8_t _gbaMode);
int8_t API_WaitForOK(void);
int8_t API_WriteRam(int8_t _gbaMode);


//side functions that can be used if the API is used in a custom manor
int8_t API_GetRom(void); //gets called by API_Get_Memory
int8_t API_GetRam(void); //gets called by API_Get_Memory
void API_Send_Abort(uint8_t type);
void API_Send_Name(void);
void API_Send_Cart_Type(void);
void API_Send_Size(void);