/*
 * adc_7seg.c
 * ECGR 5431: Lab 3
 * Group 14 - Michael Campo, Jeremie Tuzizila
 *
 * This code will enable an ADC to read voltage from a potentiometer.
 * A hex character between 0-F will be shown on a 7-segment dependent
 * on the read voltage value.  :)
 */

#include <msp430.h>

/* Global variables */
char val = '0';
unsigned int ADC_Read = 0;

 /* Function Prototypes */
char ADC_sample(void);
void display_7seg(char);

void main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog timer
    P2DIR |= 0x7F;                          // set P2 pins to output
    P2OUT = 0x00;                           // reset all P2 output pins to clear 7-seg
    P2SEL &= ~BIT6;                         // turn off XIN to enable P2.6
    P1SEL |= BIT2;                          // set P1.2 to analog input

    /* Configure ADC Channel */
    ADC10CTL1 = INCH_2 + ADC10DIV_3;        // select channel A2, ADC10CLK/3
    ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON; // sample/hold 64 cycle, multiple sample, turn on ADC10
    ADC10AE0 |= BIT2;                       // enable P1.2 for analog input


    while(1){
        char ADC_value = ADC_sample();
        display_7seg(ADC_value);
    }

}

/*
 * Function:  ADC_sample
 * ----------------------
 * Enables and starts ADC conversion. Extrapolate hex value based on
 * ADC value from ADC10MEM Register.
 *
 * returns: Character of hex number 0-F
 */

char ADC_sample(void)
{
    ADC10CTL0 |= ENC + ADC10SC;                     // enable and start conversion
    while ( (ADC10CTL1 & ADC10BUSY) == 0x01);       // wait until sample operation is complete
    ADC_Read = ADC10MEM;                            // ADC_Read from ADC register

    /* use if-else statements to determine character
     * values with a 2 point upper buffer */
    if (ADC_Read <62){
            val = '0';
    }
    else if ((ADC_Read >=64) && (ADC_Read < 126)){
            val = '1';
        }
    else if ((ADC_Read >=128) && (ADC_Read < 190)){
            val = '2';
        }
    else if ((ADC_Read >=192) && (ADC_Read < 254)){
            val = '3';
        }
    else if ((ADC_Read >=256) && (ADC_Read < 318)){
            val = '4';
        }
    else if ((ADC_Read >=320) && (ADC_Read < 382)){
            val = '5';
        }
    else if ((ADC_Read >=384) && (ADC_Read < 446)){
            val = '6';
        }
    else if ((ADC_Read >=448) && (ADC_Read < 510)){
            val = '7';
        }
    else if ((ADC_Read >=512) && (ADC_Read < 574)){
            val = '8';
        }
    else if ((ADC_Read >=576) && (ADC_Read < 638)){
            val = '9';
        }
    else if ((ADC_Read >=640) && (ADC_Read < 702)){
            val = 'a';
        }
    else if ((ADC_Read >=704) && (ADC_Read < 766)){
            val = 'b';
        }
    else if ((ADC_Read >=768) && (ADC_Read < 830)){
            val = 'c';
        }
    else if ((ADC_Read >=832) && (ADC_Read < 894)){
            val = 'd';
        }
    else if ((ADC_Read >=896) && (ADC_Read < 958)){
            val = 'e';
        }
    else if (ADC_Read >=960){
            val = 'f';
        }
    else {
        val = val;
    }

    return val;
}

/*
 * Function: display_7seg
 * --------------------
 * Based on the char value val, assign output port P2.0-P2.6
 * to display the corresponding hex character
 *
 * val: Character of hex number 0-F
 *
 * returns: void
 */

void display_7seg(char val)
{
    switch(val) {
    case '0':
        P2OUT = ~0x7E;
        break;

    case '1':
        P2OUT = ~0x30;
        break;

    case '2':
        P2OUT = ~0x6D;
        break;

    case '3':
        P2OUT = ~0x79;
        break;

    case '4':
        P2OUT = ~0x33;
        break;

    case '5':
        P2OUT = ~0x5B;
        break;

    case '6':
        P2OUT = ~0x5F;
        break;

    case '7':
        P2OUT = ~0x70;
        break;

    case '8':
        P2OUT = ~0x7F;
        break;

    case '9':
        P2OUT = ~0x7B;
        break;

    case 'a':
        P2OUT = ~0x77;
        break;

    case 'b':
        P2OUT = ~0x1F;
        break;

    case 'c':
        P2OUT = ~0x4E;
        break;

    case 'd':
        P2OUT = ~0x3D;
        break;

    case 'e':
        P2OUT = ~0x4F;
        break;

    case 'f':
        P2OUT = ~0x47;
        break;

    default:
        P2OUT = ~0x00;
    }
}
