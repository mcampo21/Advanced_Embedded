#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>

/***************************************************************************
 * accel_display.c
 * ECGR 5431: Lab 5
 * Date: 10/6/22
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
#define DP      (~BIT7)
#define MINUS   (~0x01)
#define X_AXIS  (~0x37)
#define Y_AXIS  (~0x3B)
#define TIMER_DELAY_MS  3000    // 3s delay time

/* Global variables */
unsigned int first=0, second=0, third=0, fourth=0, old_val=0;
unsigned int adc_samples[3];
unsigned int Axis = 0;          // X=0, Y=1, Z=2
unsigned int TCount = 0;

/* Function Prototypes */
void portInit(void);
void timerInit(void);
int sampleADC(int,int);
int getKey(int);
int getGravity(int);
void display_A(int,int);
void display_B(int,int);
void displayDigit(int);

int main(void)
{
    portInit();
    timerInit();
    _enable_interrupt();
    TACCR0 = 1000 - 1;      // start timer

    while(1){
        int read_val = sampleADC(old_val, Axis);
        //int keyVal = getKey(readVal);
        //display_A(keyVal, Axis);
        int gravity_val = getGravity(read_val);
        display_B(gravity_val, Axis);
        old_val = read_val;
        __delay_cycles(10000);
    }

}


/*
 * Function:    portInit
 * ---------------------
 * Initializes MCU registers and configures ADC
 */
void portInit(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog timer
    P1DIR = 0xF0;                           // set pins P1.4 - P1.7 to output
    P2DIR |= 0xFF;                          // set P2 pins to output
    P2OUT = 0x00;                           // reset all P2 output pins to clear 7-seg
    P2SEL &= ~(BIT6 + BIT7);                 // turn off XIN & XOUT to enable P2.6
    P1SEL |= 0x07;                          // set P1.0 - P1.2 to analog input

    /* Configure ADC Channels */
    ADC10CTL1 = INCH_3 + ADC10DIV_3 + CONSEQ_1; // select channel A3, CLK/3, repeat sequence of channels
    ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON;     // sample/hold 64 cycle, multiple sample, turn on ADC10
    ADC10AE0 = 0x07;                            // enable P1.0 - P1.2 for analog input
    ADC10DTC1 = 3;                              // transfer block is 3 wide
}


/*
 * Function:    timerInit
 * ---------------------
 * Initializes timer for MCU
 */
void timerInit(void)
{

    /* Configure Timer */
    TACCR0 = 0;                         // Initially, stop the timer
    TACCTL0 |= CCIE;                    // Enable interrupt for CCR0
    TACTL = TASSEL_2 + ID_0 + MC_1;     // Select SMCLK, SMCLK/1, Up Mode
}


/*
 * Function:  sampleADC
 * ----------------------
 * Enables and starts ADC conversion.
 * Return sampled value from ADC10MEM Register.
 *
 * returns: int value between 0-1023
 */
int sampleADC(int oldVal, int Axis)
{
    unsigned int sum = 0;
    unsigned int i = 0;

    for (i=0; i<32; i++){                               // sample the ADC value 32 times
        ADC10CTL0 |= ENC + ADC10SC;                     // enable and start conversion
        while ( (ADC10CTL1 & ADC10BUSY) == 0x01);       // wait until sample operation is complete
        ADC10SA = (unsigned int)adc_samples;            // send values to sample array
        sum += adc_samples[Axis];
    }

    if (sum == 0){return 0;}
    unsigned int read = (sum/32)+1;                     // take the average sampled value
    if (read == 1024){return 1023;}                     // fix top edge condition created by sampled rounding
    int difference = abs(read-old_val);                  // find the difference between the newly sampled value and previous value
    if (difference < 2){                                // if difference less than 2, return old value to help prevent oscillating
        return old_val;
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
int getKey(int read_val)
{
    unsigned int key=0;

    if (read_val > 999){
        key = 4;
    }
    else if (read_val > 99){
        key = 3;
    }
    else if (read_val > 10){
        key = 2;
    }
    else {
        key = 1;
    }


    first = read_val % 10;
    read_val /= 10;

    second = read_val % 10;
    read_val /= 10;

    third = read_val % 10;
    read_val /= 10;

    fourth =  read_val % 10;

    return key;
}

/*
 * Function:  getGravity
 * ----------------------
 * Receives the sampled ADC value and normalizes
 * the value into a range between -3 and +3.
 *
 * returns: 0 for negative and 1 for positive
 */
int getGravity(int readVal)
{
    unsigned int target = 512;
    unsigned int count = 0;

    if (readVal == 512){                    // Middle place 0
        first = 0;
        second = 0;
        return 1;
    }

    else if (readVal < 512){                                // Find negative value
        while((target > readVal) && (target > 2)){          // Continue incrementing down until target is less than ADC value
            target -= 17;                                   // Range of 17 values per 0.1
            count++;
        }
        first = count % 10;
        count /= 10;
        second = count % 10;
        return 0;
    }

    else if (readVal > 512){                     // Find positive value
        while((target < readVal) && (target < 1022)){
            target += 17;
            count++;
        }
        first = count % 10;
        count /= 10;
        second = count % 10;
        return 1;
    }

    return 0;
}

void display_A(int keyVal, int Axis)
{
    // use P2.0 - P2.7 for digit display
    // use P1.4 - P1.7 for place selection
    switch(keyVal) {                // Use key to determine display length of values

    case 1:
        P1OUT = BIT4;               // Select ones place LED
        displayDigit(first);        // Call function to display value
        break;

    case 2:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        break;

    case 3:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT = BIT6;
        displayDigit(third);
        break;

    case 4:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT = BIT6;
        displayDigit(third);
        __delay_cycles(2000);
        P1OUT = BIT7;
        displayDigit(fourth);
        break;

    default:
        P1OUT = 0x00;
    }

    __delay_cycles(2000);

    switch(Axis) {
    case 0:
        P1OUT = BIT4;
        P2OUT = DP;
        break;
    case 1:
        P1OUT = BIT5;
        P2OUT = DP;
        break;
    case 2:
        P1OUT = BIT6;
        P2OUT = DP;
        break;
    default:
        P1OUT = BIT7;
        P2OUT |= ~0;
    }
}

void display_B(int gravity_val, int Axis)
{
    // use P2.0 - P2.7 for digit display
    // use P1.4 - P1.7 for place selection
    switch(Axis){
    case 0:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT = BIT6;
        P2OUT = MINUS;
        __delay_cycles(2000);
        P1OUT = BIT7;
        P2OUT = X_AXIS;
        break;

    case 1:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT = BIT7;
        P2OUT = Y_AXIS;
        break;

    case 2:
        P1OUT = BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT = BIT7;
        P2OUT = TWO;                // Use 2 for Z-axis display
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

// Interrupts

//Timer ISR
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A_CCR0_ISR(void) {
    TCount++;
    if (TCount >= TIMER_DELAY_MS){
        if(Axis == 2){Axis = 0;}
        else{Axis++;}
        TCount = 0;                 // reset count value
    }
}

