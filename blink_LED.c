#include <msp430.h>				
/**
 * blink_LED.c
 * ECGR 5431: Lab 2
 * Group 14 - Michael Campo, Jeremie Tuzizila
 *
 * This code will blink an LED for 1 second unless the button is pressed
 * which will keep the LED in its current state. :P
 */
void main(void)
{
	WDTCTL = WDTPW | WDTHOLD;		// stop watchdog timer
	P1DIR |= BIT4;                  // configure P1.4 as output
    P1OUT |= BIT4;                  // enable led

    volatile unsigned short read = 0;

	while(1)
	{
	    read = BIT1 & P1IN;         // read value of button on P1.1
	    if (read == BIT1){
	        P1OUT ^= BIT4;          // provide output to led
	    }
	    __delay_cycles(1000000);    // delay for 1 second
	}
}


