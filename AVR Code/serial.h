/*
serial - An AVR library to communicate with a computer over UART/RS232
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

//comment this out to reduce code size, but remove support for Variadic functions like in cprintf
#define _VA_SUPPORT

#ifdef SAVE_SPACE
#undef _VA_SUPPORT
#endif

#ifdef _VA_SUPPORT
#define CPRINTF_STR_NAME cprintf_string
#else
#define CPRINTF_STR_NAME cprintf
#endif

void initConsole(void);
void setRecvCallback(void* cb);
unsigned char Serial_ReadByte(void);
void cprintf_char( unsigned char text );

#ifdef _VA_SUPPORT
void cprintf( char *text,... ); 
#endif 

void CPRINTF_STR_NAME(char* str);
