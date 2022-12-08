#include <msp430.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/***************************************************************************
 * adc_ultrasonic_sensor.c
 * ECGR 5431: Lab 9
 * Date: 10/07/22
 * Group 14: Michael Campo, Jeremie Tuzizila
 *
 * This code will capture a distance value using an ultrasonic sensor and
 * activate a speaker if specific thresholds are measured. The measured
 * distance is also sent to secondary MCU via UART to display the value on
 * the LED display. When in level mode, the accelerometer measures the angle
 * in the X and Y axis directions and displays the value on the LED.
 *
 ***************************************************************************/

/* Define 7-seg to hex values, use not operator if display is common anode*/
#define _A BIT6
#define _B BIT4
#define _C BIT1
#define _D BIT0
#define _E BIT3
#define _F BIT5
#define _G BIT2

#define ZERO    _G
#define ONE     _A + _D + _E + _F +_G
#define TWO     _F + _C
#define THREE   _E + _F
#define FOUR    _A + _D + _E
#define FIVE    _B + _E
#define SIX     _B
#define SEVEN   _D + _E + _F + _G
#define EIGHT   0
#define NINE    _E
#define ECHO_P  (BIT1)
#define TRIG_P  (BIT0)

#define BUF_SIZE 7

#define X_MIN 405
#define X_MID 505
#define X_MAX 605

#define Y_MIN 415
#define Y_MID 515
#define Y_MAX 615

#define Z_MIN 426
#define Z_MID 526
#define Z_MAX 626

/* Global variables */
volatile unsigned int Start;
volatile unsigned int End;
volatile unsigned int Travel_time;
volatile unsigned int Distance = 0;
volatile unsigned int Level = 0;
volatile unsigned int Counting = 0;
volatile unsigned int Edge = 0;
static char TxBuffer[BUF_SIZE], RxBuffer[BUF_SIZE], Digits[BUF_SIZE];
unsigned int TxBufIndex = 0, RxBufIndex = 0;
unsigned int data0[10], data1[10], data2[10], data3[10], data4[10];
volatile unsigned int myPresetDistances[5] = { 5, 25, 60, 100, 220 }; // Preset Default
volatile unsigned int mySortedDistances[5] = { 5, 10, 15, 20, 25 }; // Sorted Default
volatile unsigned int myPresetDistancesIndex = 0;
volatile unsigned int adc_samples[8];
volatile int x, y, z;
volatile unsigned int thetaX, thetaY;

enum System
{
    DISTANCE, ANGLE
};
volatile enum System System = ANGLE;
enum Bool
{
    FALSE, TRUE
};
volatile enum Bool Sort = FALSE;
enum Flags
{
    STOP, SET, SAVE
};
volatile enum Flags Flag = STOP;

/* Function Prototypes */
int hwFlag(void);
void portInit0(void);
void portInit1(void);
void setSpeaker(void);
void setDistance(int);
void convertSensor(int);
void transmit(void);
void receive(void);
void display(void);
void displayDigit(char);
void triggerSensor(void);
int avg(unsigned int*, unsigned int);

int main(void)
{
    int mcu = hwFlag();
    if (mcu == 0)
    { // Activate Measuring code on MCU0
        portInit0();
        while (1)
        {
            if (System == DISTANCE)
            {
                Digits[5] = 'D';                            // System Mode Flag
                triggerSensor();                            // Capture ultrasonic measurements
                convertSensor(Distance);                    // Convert sensor value into char

                if (myPresetDistances[myPresetDistancesIndex] == Distance)
                {
                    setDistance(Distance);                  // Sets distance based off of sensor value
                }
                else
                {
                    Level = 0;
                }

            }
            else
            {
                Digits[5] = 'A';                            // System Mode Flag
                ADC10CTL0 &= ~ENC;
                while ((ADC10CTL1 & ADC10BUSY));            // wait until sample operation is complete
                ADC10CTL0 |= ENC + ADC10SC;                 // enable and start conversion
                ADC10SA = (unsigned int) adc_samples;       // send values to sample array

                x = abs(avg(data0, adc_samples[2]) - X_MID);    // take absolute value of X axis
                y = abs(avg(data1, adc_samples[6]) - Y_MID);    // take absolute value of Y axis
                z = abs(avg(data2, adc_samples[4]) - Z_MID);

                if (x < 98)
                {
                    thetaX = asinf(((float) (x) / 97)) * 180 * M_1_PI;
                }
                else
                {
                    thetaX = 90;
                }

                if (y < 98)
                {
                    thetaY = asinf(((float) (y) / 97)) * 180 * M_1_PI;
                }
                else
                {
                    thetaY = 90;
                }

                convertSensor((thetaX * 100) + thetaY);

                if ((thetaX * 100 + thetaY) == 0)
                {
                    Level = 5;
                }
                else
                {
                    Level = 0;
                }

            }

            setSpeaker();                   // Sets speaker output
            transmit();                     // Send converted char's through UART
            __delay_cycles(100000);         // Sample every 200ms or 5Hz frequency
        }
    }
    if (mcu == 1)
    {                                       // Activate Display code on MCU1
        portInit1();
        while (1)
        {
            if (Flag == SAVE)
            {
                receive();
                IE2 |= UCA0RXIE;            // Enable USCI_A0 RX interrupt
                Flag = STOP;
            }
            else
            {
                display();                  // Display the distance value
            }
        }
    }
}

/*
 * Function: avg
 * ---------------------
 * This function return the average of 10 running samples.
 */
int avg(unsigned int *arr, unsigned int val)
{
    arr[9] = arr[8];
    arr[8] = arr[7];
    arr[7] = arr[6];
    arr[6] = arr[5];
    arr[5] = arr[4];
    arr[4] = arr[3];
    arr[3] = arr[2];
    arr[2] = arr[1];
    arr[1] = arr[0];
    arr[0] = val;

    val = arr[0] + arr[1] + arr[2] + arr[3] + arr[4] + arr[5] + arr[6] + arr[7]
            + arr[8] + arr[9];
    val = val / 10;

    return val;
}

/*
 * Function: hwFlag
 * ---------------------
 * Reads pin 1.0 to determine which code function should be read on the microcontroller.
 * Acts as a hardware flag.
 */
int hwFlag(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    P1DIR &= ~BIT0;
    P1REN = BIT0;
    P1OUT = 0;
    return P1IN & BIT0;
}

/*
 * Function: triggerSensor
 * ---------------------
 * Toggles the trigger pin to be on for 10us. Calculates the distance value
 * and averages the measurements to increase accurately.
 */
void triggerSensor(void)
{
    TA1CTL |= TACLR;
    P2OUT |= TRIG_P;
    __delay_cycles(10); // 10 us
    P2OUT &= ~TRIG_P;

    unsigned int val = 0;

    val = avg(data1, Travel_time / 58);

    if (val > 400)
    {
        Distance = 400;         // Maximum range of 400m
    }
    else
    {
        Distance = val;
    }
}

/*
 * Function: convertSensor
 * ---------------------
 * Receives the sampled ultrasonic sensor value and splits it
 * into digit places using modulo division.
 * Based off position place a key is created.
 * Values stored into global array.
 */
void convertSensor(int readVal)
{
    unsigned int key = 0;
    if (readVal > 999)
    {                 // Capture 1000s place
        key = 4;
    }
    else if (readVal > 99)
    {                 // Capture 100s place
        key = 3;
    }
    else if (readVal > 9)
    {              // Capture 10s place
        key = 2;
    }
    else
    {
        key = 1;
    }
    Digits[0] = (readVal % 10) + 48;
    readVal /= 10;
    Digits[1] = (readVal % 10) + 48;
    readVal /= 10;
    Digits[2] = (readVal % 10) + 48;
    readVal /= 10;
    Digits[3] = (readVal % 10) + 48;

    if (System == DISTANCE)
    {
        Digits[4] = key + 48;
    }
    else
    {
        Digits[4] = 4 + 48;
    }
    Digits[6] = ','; // add , as stop flag
}

/*
 * Function: setDistance
 * ---------------------
 * Sets the distance level for the speaker based off of selected threshold values
 */
void setDistance(int readVal)
{
    if (readVal == myPresetDistances[0])
    {
        Level = 5;
    }
    else if (readVal == myPresetDistances[1])
    {
        Level = 4;
    }
    else if (readVal == myPresetDistances[2])
    {
        Level = 3;
    }
    else if (readVal == myPresetDistances[3])
    {
        Level = 2;
    }
    else if (readVal == myPresetDistances[4])
    {
        Level = 1;
    }
    else
    {
        Level = 0;
    }
}

/*
 * Function: setSpeaker
 * ---------------------
 * Sets the interrupt timer values for the PWM to play certain notes based off of the distance level
 */
void setSpeaker()
{
    P2SEL &= ~BIT2;
    switch (Level)
    {
    case 1:
        TA0CCR0 = 12134; // E2
        TA0CCR1 = 6067;
        P2SEL |= BIT2;
        break;
    case 2:
        TA0CCR0 = 3405; // D3
        TA0CCR1 = 1702;
        P2SEL |= BIT2;
        break;
    case 3:
        TA0CCR0 = 2272; // A4
        TA0CCR1 = 1136;
        P2SEL |= BIT2;
        break;
    case 4:
        TA0CCR0 = 1516; // E5
        TA0CCR1 = 758;
        P2SEL |= BIT2;
        break;
    case 5:
        TA0CCR0 = 1136; // A6
        TA0CCR1 = 568;
        P2SEL |= BIT2;
        break;
    default:
        TA0CCR0 = 0;
        TA0CCR1 = 0;
        P2SEL |= BIT2;
    }
}

/*
 * Function: transmit
 * ----------------------
 * Transfers converted ADC data to Tx buffer and begins transmission
 */
void transmit()
{
    unsigned int i;
    for (i = 0; i < sizeof(TxBuffer); i++)
    {
        TxBuffer[i] = Digits[i];
    }
    TxBufIndex = 0;
    IE2 |= UCA0TXIE;                        // Enable USCI_A0 TX interrupt to begin UART transmission
}

// UART TX ISR to transmit data
#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
    UCA0TXBUF = TxBuffer[TxBufIndex];
    TxBufIndex++;                           // Transmit next character
    if (TxBufIndex >= sizeof(TxBuffer))
    {                                       // Check if TX has been completed
        TxBufIndex = 0;
        IE2 &= ~UCA0TXIE;                   // Disable USCI_A0 TX interrupt
    }
}

// UART RX ISR to receive data
#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
    if (RxBufIndex < sizeof(Digits))
    {
        if (UCA0RXBUF == ',')
        {
            RxBufIndex = 0;
            Flag = SAVE;
            IE2 &= ~UCA0RXIE; // Disable USCI_A0 RX interrupt
        }
        else
        {
            RxBuffer[RxBufIndex] = UCA0RXBUF;
            RxBufIndex++;
        }
    }
    else
    {
        RxBufIndex = 0;
    }
}

/*
 * Function: receive
 * ----------------------
 * Stores received UART data from buffer into an array
 */
void receive()
{
    unsigned int i;
    for (i = 0; i < sizeof(Digits); i++)
    {
        Digits[i] = RxBuffer[i];
    }
}

/*
 * Function: display
 * ----------------------
 * Enables and starts ADC conversion.
 * Return sampled value from ADC10MEM Register.
 *
 * returns: int value between 0-1023
 */
void display(void)
{
// use P2.0 - P2.7 for digit display
// use P1.0 - P1.3 for place selection
    char keyVal;
    keyVal = Digits[4];
    switch (keyVal)
    {
    case '1':
        P1OUT = BIT4;               // Set digit position on
        displayDigit(Digits[0]);    // Display number
        __delay_cycles(4000);
        break;
    case '2':
        P1OUT = BIT4;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT6;
        displayDigit(Digits[1]);
        __delay_cycles(4000);
        break;
    case '3':
        P1OUT = BIT4;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT6;
        displayDigit(Digits[1]);
        __delay_cycles(4000);
        P1OUT = BIT7;
        displayDigit(Digits[2]);
        __delay_cycles(4000);
        break;
    case '4':
        P1OUT = BIT4;
        displayDigit(Digits[0]);
        __delay_cycles(2000);
        P1OUT = BIT6;
        displayDigit(Digits[1]);
        __delay_cycles(2000);
        P1OUT = BIT7;
        displayDigit(Digits[2]);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(Digits[3]);
        __delay_cycles(4000);
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
void displayDigit(char val)
{
    switch (val)
    {                       // Use char to select display bit value
    case '0':
        P2OUT = ZERO;       // Set pins to display value
        break;
    case '1':
        P2OUT = ONE;
        break;
    case '2':
        P2OUT = TWO;
        break;
    case '3':
        P2OUT = THREE;
        break;
    case '4':
        P2OUT = FOUR;
        break;
    case '5':
        P2OUT = FIVE;
        break;
    case '6':
        P2OUT = SIX;
        break;
    case '7':
        P2OUT = SEVEN;
        break;
    case '8':
        P2OUT = EIGHT;
        break;
    case '9':
        P2OUT = NINE;
        break;
    default:
        P2OUT = ~0x00;
    }
}

/*
 * ISR: Timer0 A0 Interrupt service routine
 * --------------------
 * if rising edge -> capture the timer register when pulse starts
 * else if falling edge -> capture the timer register when pulse ends
 */
#pragma vector = TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void)
{
    Edge = CCI;
    switch (TA1IV)
    {
    case TA1IV_NONE:
        break;
    case 10:
        // Don't sample if overflowed
        break;
    case TA1IV_TACCR1:
        if (TA1CCTL1 & Edge)
        { // start timer
            Start = TA1CCR1;
        }
        else
        { // stop timer
            End = TA1CCR1;
            Travel_time = End - Start; // Calculate the travel time
        }
        break;
    default:
        break;
    }
    TACTL &= ~CCIFG; // clear interrupt flag
}

/*
 * ISR: Port 2 Interrupt service routine
 * --------------------
 * if rising edge -> capture the timer register when pulse starts
 * else if falling edge -> capture the timer register when pulse ends
 */
#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR(void)
{
    if ((P2IFG & BIT4))
    {
        if (myPresetDistancesIndex == 4)
        {
            myPresetDistancesIndex = 0;
        }
        else
        {
            myPresetDistancesIndex++;
        }

        P2IFG &= ~BIT4;                     // clear P2.4 interrupt flag
    }

    if (P2IFG & BIT5)
    {
        if (System == DISTANCE)
        {
            System = ANGLE;
        }
        else
        {
            System = DISTANCE;
        }
        P2IFG &= ~BIT5;                     // clear P2.5 interrupt flag
    }
}

/*
 * Function: portInit0
 * ---------------------
 * Initializes microcontroller 0 registers and configures ADC
 */
void portInit0(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // stop watchdog timer
    DCOCTL = 0;                             // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                  // Set DCO
    DCOCTL = CALDCO_1MHZ;

    /* Configure UART */
    P1SEL = BIT2;                           // P1.2=TXD
    P1SEL2 = BIT2;                          // P1.2=TXD
    UCA0CTL1 |= UCSSEL_2;                   // SMCLK
    UCA0BR0 = 104;                          // 1MHz 9600
    UCA0BR1 = 0;                            // 1MHz 9600
    UCA0MCTL = UCBRS0;                      // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                   // **Initialize USCI state machine**

    /* Set GPIO Pin */
    P2DIR |= BIT3 + BIT6;                   // Set P1.6 as speaker driver
    P2SEL |= BIT6;
    P2SEL &= ~BIT7;
    P2SEL2 &= BIT6 + BIT7;                  // turn off XIN to enable P2.6

    /* Configure PWM Timer */
    TACTL = TASSEL_2 + MC_1;                // SMCLK, upmode
    TA0CCR0 = 12134;                        // PWM period
    TA0CCTL1 = OUTMOD_7;                    // CCR0 reset/set
    TA0CCR1 = 6067;                         // PWM Duty Cycle

    /* Configure Ultrasonic */
    P2DIR |= TRIG_P;                        // Set P2.0 output as trigger
    P2OUT &= ~TRIG_P;                       // Set P2.0 off
    P2DIR &= ~ECHO_P;                       // Set P2.1 input as echo / 1A3_CCI1A
    P2SEL |= ECHO_P;
    P2REN |= ECHO_P;                        // Enable resistor
    P2OUT &= ~ECHO_P;                       // Pull-down mode

    /* Configure Ultrasonic Timer */
    TA1CTL = MC_0;
    TA1CCTL1 |= SCS + CM_3 + CCIS_0 + CAP + CCIE; // synchronous, rising/falling edge, compare input select, capture mode, interrupt enable
    TA1CTL |= TASSEL_2 + MC_2;

    /*  Configure Button as interrupt  */
    // SYSTEM SW
    P2REN |= BIT4;
    P2OUT |= BIT4;
    P2IE |= BIT4;                           // Set button interrupt to P2.4
    P2IES &= ~(BIT4);

    // MODE SW
    P2REN |= BIT5;
    P2OUT |= BIT5;
    P2IE |= BIT5;
    P2IES &= ~(BIT5);
    P2IFG = 0x00;                  // clear interrupt flags
    P2OUT &= ~BIT3;

    /* Configure ADC Channels */
    ADC10CTL1 = INCH_7 + ADC10DIV_0 + CONSEQ_3 + SHS_0;
    ADC10CTL0 = SREF_0 + ADC10SHT_2 + MSC + ADC10ON;
    ADC10AE0 = BIT7 + BIT6 + BIT5 + BIT4 + BIT3 + BIT1;
    ADC10DTC1 = 8;

    __bis_SR_register(GIE); // interrupts enabled
}

/*
 * Function: portInit1
 * ---------------------
 * Initializes microcontroller 1 registers and configures ADC
 */
void portInit1(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    DCOCTL = 0;                 // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;      // Set DCO
    DCOCTL = CALDCO_1MHZ;

    /* Configure UART */
    P1SEL = BIT1 + BIT2;        // P1.1=RXD
    P1SEL2 = BIT1 + BIT2;       // P1.1=RXD
    UCA0CTL1 |= UCSSEL_2;       // SMCLK
    UCA0BR0 = 104;              // 1MHz 9600
    UCA0BR1 = 0;                // 1MHz 9600
    UCA0MCTL = UCBRS0;          // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;       // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;            // Enable USCI_A0 RX interrupt

    /* Configure GPIO */
    P1DIR |= (BIT6 + BIT7 + BIT4 + BIT5);   // set P1.2-P1.5 pins to output
    P2DIR |= 0x7F;                          // set P2 pins to output
    P2OUT = 0x00;                           // reset all P2 output pins to clear 7-seg
    P2SEL &= ~(BIT6 + BIT7);                // turn off XIN to enable P2.6
    __bis_SR_register(GIE);                 // interrupts enabled
}
