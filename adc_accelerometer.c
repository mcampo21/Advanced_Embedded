#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>

/***************************************************************************
 * adc_accelerometer.c
 * ECGR 5431: Lab 5
 * Date: 10/6/22
 * Group 14: Michael Campo, Jeremie Tuzizila
 *
 * This code will enable an ADC to read voltage from an accelerometer.
 * The value of this will either be displayed as raw sampled from the ADC
 * or it will be converted into a gravity value in accordance to the
 * accelerometer sensitivity. A button can be pressed to switch between modes.
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
#define MEDIAN  479                 // zero bias value for accelerometer
#define TIMER_DELAY_MS  3000        // 3s delay time

/* Global variables */
unsigned int first=0, second=0, third=0, fourth=0, old_val=0;
unsigned int adc_samples[8];
unsigned int Axis = 0;              // X=0, Y=1, Z=2
unsigned int TCount = 0;
unsigned int DisplayState = 0;      // picks state A=0, B=1

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
    portInit();             // initialize ports
    timerInit();            // initialize timer
    _enable_interrupt();
    TACCR0 = 1000 - 1;      // start timer

    while(1){
        int read_val = sampleADC(old_val, Axis);        // sample ADC value from accelerometer

        if (~DisplayState) {                            // DisplayState acts as flag to determine display mode
            int keyVal = getKey(read_val);              // split raw ADC into values place
            display_A(keyVal, Axis);                    // display raw ADC value

        }
        else{
            int gravity_val = getGravity(read_val);     // convert raw ADC to gravity value
            display_B(gravity_val, Axis);               // display axis and gravity value
        }

        old_val = read_val;                             // store current value for comparison
        __delay_cycles(1000);
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
    P1DIR = 0x17;                           // set pins P1.0-P1.2, P1.4 to output
    P2DIR |= 0xFF;                          // set P2 pins to output
    P2OUT = 0x00;                           // reset all P2 output pins to clear 7-seg
    P2SEL &= ~(BIT6 + BIT7);                // turn off XIN & XOUT to enable P2.6
    P1SEL |= (BIT5 + BIT6 + BIT7);          // set P1.5 - P1.7 to analog input

    /* Configure ADC Channels */
    ADC10CTL1 = INCH_7 + ADC10DIV_3 + CONSEQ_1; // select channel A7, CLK/3, repeat sequence of channels
    ADC10CTL0 = ADC10SHT_3 + MSC + ADC10ON;     // sample/hold 64 cycle, multiple sample, turn on ADC10
    ADC10AE0 = (BIT5 + BIT6 + BIT7);            // enable P1.5 - P1.7 for analog input
    ADC10DTC1 = 7;                              // transfer block is 7 wide

    /*  Configure Button as interrupt  */
    P1REN |= BIT3;
    P1OUT |= BIT3;
    P1IE |= BIT3;
    P1IES &= ~(BIT3);
    P1IFG = 0x00;                           // clear interrupt flags
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
    unsigned int target = MEDIAN;
    unsigned int count = 0;

    if ((readVal > MEDIAN-4) && (readVal < MEDIAN+4)){                    // Middle place 0
        first = 0;
        second = 0;
        return 1;
    }

    else if (readVal < MEDIAN){                             // Find negative value
        while((target > readVal) && (target > 2)){          // Continue incrementing down until target is less than ADC value
            target -= 9;                                    // Range of 17 values per 0.1
            count++;
        }
        first = count % 10;
        count /= 10;
        second = count % 10;
        return 0;
    }

    else if (readVal > MEDIAN){                     // Find positive value
        while((target < readVal) && (target < 1022)){
            target += 10;
            count++;
        }
        first = count % 10;
        count /= 10;
        second = count % 10;
        return 1;
    }

    return 0;
}

/*
 * Function:  display_A
 * ----------------------
 * This function is used to display the raw ADC values
 * as required by part A of the lab.
 *
 */
void display_A(int keyVal, int Axis)
{
    P1OUT &= BIT3;
    // use P2.0 - P2.7 for digit display
    // use P1.4 - P1.7 for place selection
    switch(keyVal) {                // Use key to determine display length of values

    case 1:
        P1OUT |= BIT4;               // Select ones place LED
        displayDigit(first);        // Call function to display value
        break;

    case 2:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        break;

    case 3:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT ^= BIT2;
        P1OUT |= BIT1;
        displayDigit(third);
        break;

    case 4:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        __delay_cycles(2000);
        P1OUT ^= BIT2;
        P1OUT |= BIT1;
        displayDigit(third);
        __delay_cycles(2000);
        P1OUT ^= BIT1;
        P1OUT |= BIT0;
        displayDigit(fourth);
        break;

    default:
        P1OUT &= BIT3;
    }

    __delay_cycles(2000);
    P1OUT ^= BIT1;

    switch(Axis) {
    case 0:
        P1OUT |= BIT4;      // Select decimal point location
        P2OUT = DP;         // display decimal point
        break;
    case 1:
        P1OUT |= BIT2;
        P2OUT = DP;
        break;
    case 2:
        P1OUT |= BIT1;
        P2OUT = DP;
        break;
    default:
        P1OUT |= BIT7;
        P2OUT |= ~0;
    }
}

/*
 * Function:  display_B
 * ----------------------
 * This function is used to display the gravity values
 * as required by part B of the lab.
 *
 */
void display_B(int gravity_val, int Axis)
{
    unsigned int sign = 0xFF;       // create sign flag with value
    if (gravity_val == 0){          // if gravity flag is negative (0), display MINUS sign
        sign = MINUS;
    }
    P1OUT &= BIT3;

    switch(Axis){
    case 0:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT ^= BIT2;
        P1OUT |= BIT1;
        P2OUT = sign;
        __delay_cycles(2000);
        P1OUT ^= BIT1;
        P1OUT |= BIT0;
        P2OUT = X_AXIS;
        break;

    case 1:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT ^= BIT2;
        P1OUT |= BIT1;
        P2OUT = sign;
        __delay_cycles(2000);
        P1OUT ^= BIT1;
        P1OUT |= BIT0;
        P2OUT = Y_AXIS;
        break;

    case 2:
        P1OUT |= BIT4;
        displayDigit(first);
        __delay_cycles(2000);
        P1OUT ^= BIT4;
        P1OUT |= BIT2;
        displayDigit(second);
        P2OUT &= DP;
        __delay_cycles(2000);
        P1OUT ^= BIT2;
        P1OUT |= BIT1;
        P2OUT = sign;
        __delay_cycles(2000);
        P1OUT ^= BIT1;
        P1OUT |= BIT0;
        P2OUT = TWO;                // Use 2 for Z-axis display
        break;

    default:
        P1OUT &= BIT3;
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
// Port 1 ISR
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void) {
    DisplayState = ~(DisplayState);
    __delay_cycles(10000);
    P1IFG &= ~BIT3;                     // clear P1.3 interrupt flag
}

