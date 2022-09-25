#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>

/***************************************************************************
 * adc_4seg_display.c
 * ECGR 5431: Lab 3
 * Date: 9/22/22
 * Group 14: Michael Campo, Jeremie Tuzizila
 *
 * This code will enable an ADC to read voltage from a potentiometer.
 * A digit of the sampled value between 0-1023 will be shown on a 7-segment
 * display dependent on the read voltage value in its proper digit place. :)
 *
 ***************************************************************************/

/* Define 7-seg to hex values, use not operator if display is common anode*/
#define ZERO    (~0x7E)
#define ONE     (~0x30)
#define TWO     (~0x6D)
#define THREE   (~0x79)
#define FOUR    (~0x33)
#define FIVE    (~0x5B)
#define SIX     (~0x5F)
#define SEVEN   (~0x70)
#define EIGHT   (~0x7F)
#define NINE    (~0x7B)

/* Global variables */
unsigned int first=0, second=0, third=0, fourth=0, oldVal=0;

/* Function Prototypes */
void portInit(void);
int sampleADC(int);
int getKey(int);
void display(int);
void displayDigit(int);

int main(void)
{
    portInit();

    while(1){
        int readVal = sampleADC(oldVal);
        int keyVal = getKey(readVal);
        display(keyVal);
        oldVal = readVal;
        __delay_cycles(10000);
    }

}

/*
 * Function:    portInit
 * ---------------------
 * Initializes microcontroller registers and configures ADC
 */
void portInit(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog timer
    P1DIR |= 0x0F;                          // set pins P1.0 - P1.3 to output
    P2DIR |= 0x7F;                          // set P2 pins to output
    P2OUT = 0x00;                           // reset all P2 output pins to clear 7-seg
    P2SEL &= ~BIT6;                         // turn off XIN to enable P2.6
    P1SEL |= BIT5;                          // set P1.5 to analog input

    /* Configure ADC Channel */
    ADC10CTL1 = INCH_5 + ADC10DIV_3;        // select channel A5, ADC10CLK/3
    ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON; // sample/hold 64 cycle, multiple sample, turn on ADC10
    ADC10AE0 |= BIT5;                       // enable P1.5 for analog input
}

/*
 * Function:  sampleADC
 * ----------------------
 * Enables and starts ADC conversion.
 * Return sampled value from ADC10MEM Register.
 *
 * returns: int value between 0-1023
 */

int sampleADC(int oldVal)
{
    unsigned int sum = 0;
    unsigned int i = 0;

    __delay_cycles(1000);
    for (i=0; i<32; i++){                               // sample the ADC value 32 times
        ADC10CTL0 |= ENC + ADC10SC;                     // enable and start conversion
        while ( (ADC10CTL1 & ADC10BUSY) == 0x01);       // wait until sample operation is complete
        sum += ADC10MEM;
    }


    if (sum == 0){return 0;}

    unsigned int read = (sum/32)+1;                     // take the average sampled value

    if (read == 1024){return 1023;}                     // fix top edge condition created by sampled rounding

    int difference = abs(read-oldVal);                  // find the difference between the newly sampled value and previous value

    if (difference < 2){                                // if difference less than 2, return old value to help prevent oscillating
        return oldVal;
    }

    return read;
}

/*
 * Function:  getKey
 * ----------------------
 * Receives the sampled ADC value and splits it
 * into digit places using modulo division.
 * Based off position place a key is created
 *
 * returns: key used for digit places activated
 */

int getKey(int readVal)
{
    unsigned int key=0;

    if (readVal > 999){
        key = 4;
    }
    else if (readVal > 99){
        key = 3;
    }
    else if (readVal > 10){
        key = 2;
    }
    else {
        key = 1;
    }


    first = readVal % 10;
    readVal /= 10;

    second = readVal % 10;
    readVal /= 10;

    third = readVal % 10;
    readVal /= 10;

    fourth =  readVal % 10;

    return key;
}

/*
 * Function:  display
 * ----------------------
 * Enables and starts ADC conversion.
 * Return sampled value from ADC10MEM Register.
 *
 * returns: int value between 0-1023
 */

void display(int keyVal)
{
    // use P2.0 - P2.7 for digit display
    // use P1.0 - P1.3 for place selection
    switch(keyVal) {

    case 1:
        P1OUT = BIT0;
        displayDigit(first);
        break;

    case 2:
        P1OUT = BIT0;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT1;
        displayDigit(second);
        break;

    case 3:
        P1OUT = BIT0;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT1;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT = BIT2;
        displayDigit(third);
        break;

    case 4:
        P1OUT = BIT0;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT1;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT = BIT2;
        displayDigit(third);
        __delay_cycles(2000);
        P1OUT = BIT3;
        displayDigit(fourth);
        break;

    default:
        P1OUT = 0x00;
    }
}

/*
 * Function: displayDigit
 * --------------------
 * Based on the passed int value, assign output port P2.0-P2.6
 * to display the corresponding number
 *
 * returns: void
 */

void displayDigit(int val)
{
    switch(val) {
    case 0:
        P2OUT = ZERO;
        break;

    case 1:
        P2OUT = ONE;
        break;

    case 2:
        P2OUT = TWO;
        break;

    case 3:
        P2OUT = THREE;
        break;

    case 4:
        P2OUT = FOUR;
        break;

    case 5:
        P2OUT = FIVE;
        break;

    case 6:
        P2OUT = SIX;
        break;

    case 7:
        P2OUT = SEVEN;
        break;

    case 8:
        P2OUT = EIGHT;
        break;

    case 9:
        P2OUT = NINE;
        break;

    default:
        P2OUT = ~0x00;
    }
}
