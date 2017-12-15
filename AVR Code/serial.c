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

//includes
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h> 
#include <stdarg.h>
#include <stdio.h>
#include "serial.h"

// define some macros
#define _BUFFER_SIZE 64

//comment this out to reduce code size, but remove support for Variadic functions like in cprintf
#define _VA_SUPPORT

//Connection info
#define BAUD 9600                                   // define baud
#define BAUDRATE ((F_CPU)/(BAUD*16UL)-1)			// set baud rate value for UBRR

char _init = 0;


//callbacks
void (*cb_recv)(char); 

//Override the Recv callback, or return it to default by giving NULL
void setRecvCallback(void* cb)
{
	cb_recv = cb;
}

//Send a Char to the Other side
void usart_SendChar(char data) {
    // Wait for empty transmit buffer
    while ( !(UCSRA & (_BV(UDRE))) );
    // Start transmission
    UDR = data;
}

//Send a string to the other side
void usart_SendString(char *s) {
    // loop through entire string
    while (*s) {
        usart_SendChar(*s);
        s++; 
    }
}

//Wait untill a single Char is received
//kinda useless as we have the interrupt xD
unsigned char usart_GetChar(void) {
	if(_init == 0)
		return 0;
    // Wait for incoming data
    while ( !(UCSRA & (_BV(RXC))) );
    // Return the data
    return UDR;
}       

//Inialise the Console			
void initConsole(void) {
    // Set baud rate
    UBRRH = (uint8_t)(BAUDRATE>>8);
    UBRRL = (uint8_t)BAUDRATE;
    // Enable receiver and transmitter
    UCSRB = (1<<RXEN)|(1<<TXEN);
	//enable the receive interrupt
	//TODO : make function to enable and disable it, to replace the cli & sei in code
	UCSRB |= (1 << RXCIE);
    // Set frame format: 8data, 1stop bit
    UCSRC = (1<<URSEL)|(3<<UCSZ0);	
	
	cb_recv = usart_SendChar;
	
	//enable interrupts in the chip, cli(); disables them again
	sei();
	
	_init = 1;
	return;
}
//Wait and retrieve 1 byte
unsigned char Serial_GetByte(void)
{
	return usart_GetChar();
}             

//Print a full string
void cprintf_string(char* str)
{
	if(_init == 0)
		return;
	usart_SendString(str);
	
}

//Print a single character
void cprintf_char( unsigned char text )
{
	if(_init == 0)
		return;
	usart_SendChar(text);
}

//Print a string
void cprintf( char *text,... )
{
	if(_init == 0)
		return;
		
	char *output = text;
#ifdef _VA_SUPPORT
	char astr[_BUFFER_SIZE+1];
	memset(astr,0,_BUFFER_SIZE+1);
	
	va_list ap;
	va_start(ap,text);

	vsnprintf( astr, _BUFFER_SIZE,text,ap);
	va_end(ap);
	
	output = astr;
	
#endif

	usart_SendString(output);
}

//Interrupt handler of receiving characters
ISR(USART_RXC_vect)
{
	char ReceivedByte;
	//Put received char into the variable
	//we always need to do this, or the interrupt will keep triggering
	ReceivedByte = UDR;

	//Process the given character
	if(cb_recv != NULL)
	{
		cb_recv(ReceivedByte);
	}
}