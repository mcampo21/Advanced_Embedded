#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>

/***************************************************************************
 * adc_ultrasonic_sensor.c
 * ECGR 5431: Lab 8
 * Date: 10/07/22
 * Group 14: Michael Campo, Jeremie Tuzizila
 *
 * This code will capture a distance value using an ultrasonic sensor and
 * activate a speaker if specific thresholds are measured. The measured
 * distance is also sent to secondary MCU via UART to display the value on
 * the LED display.
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
#define ECHO_P  (BIT1)
#define TRIG_P  (BIT0)

/* Global variables */
volatile unsigned int Start;
volatile unsigned int End;
volatile unsigned int Travel_time;
volatile unsigned int Distance = 0;
volatile unsigned int Level = 0;
volatile unsigned int Counting = 0;
volatile unsigned int Edge = 0;
static char TxBuffer[5], RxBuffer[5], Digits[5];
unsigned int TxBufIndex = 0, RxBufIndex = 0;
volatile unsigned int data[10];
static unsigned int val = 0;
enum Flags {STOP, SET, SAVE};
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

int main(void)
{
    int mcu = hwFlag();

    if (mcu == 0){                                          // Activate Measuring code on MCU0
        portInit0();

        while(1){
            triggerSensor();                                // Capture ultrasonic measurements
            convertSensor(Distance);                        // Convert sensor value into char
            setDistance(Distance);                          // Sets distance based off of sensor value
            setSpeaker();                                   // Sets speaker output
            transmit();                                     // Send converted char's through UART
            __delay_cycles(100000);                         // Sample every 200ms or 5Hz frequency

        }
    }

    if (mcu == 1){                                          // Activate Display code on MCU1
        portInit1();

        while(1){
            if (Flag == SAVE){
                receive();
                IE2 |= UCA0RXIE;                            // Enable USCI_A0 RX interrupt
                Flag = STOP;
            }
            else{
                display();                                  // Display the distance value
            }
        }
    }
}

/*
 * Function:    hwFlag
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
    __delay_cycles(10);                     // 10 us
    P2OUT &= ~TRIG_P;

    unsigned int i;
    val = 0;

    for (i = sizeof data; i > 0; i--)
        {
            data[i] = data[i - 1];
        }

        data[0] = Travel_time/58;

        val = data[0] + data[1] + data[2] + data[3] + data[4] + data[5] + data[6]
                + data[7] + data[8] + data[9];

        val = val/10;

        if (val > 400){                     // Maximum range of 400m
            Distance = 400;
        }
        else{
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

    if (readVal > 99){                      // Capture 100s place
        key = 3;
    }
    else if (readVal > 9){                  // Capture 10s place
        key = 2;
    }
    else{
        key = 1;
    }

    Digits[0] = (readVal % 10) + 48;
    readVal /= 10;

    Digits[1] = (readVal % 10) + 48;
    readVal /= 10;

    Digits[2] = (readVal % 10) + 48;

    Digits[3] = key + 48;

    Digits[4] = 44;                         // add , as stop flag

}

/*
 * Function: setDistance
 * ---------------------
 * Sets the distance level for the speaker based off of selected threshold values
*/
void setDistance(int readVal)
{
    if (readVal == 5){
        Level = 5;
    }
    else if (readVal == 10){
        Level = 4;
    }
    else if (readVal == 15){
        Level = 3;
    }
    else if (readVal == 20){
        Level = 2;
    }
    else if (readVal == 25){
        Level = 1;
    }
    else{
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

    switch(Level)
    {
    case 1:
        TA0CCR0 = 12134;      // E2
        TA0CCR1 = 6067;
        P2SEL |= BIT2;
        break;
    case 2:
        TA0CCR0 = 3405;       // D3
        TA0CCR1 = 1702;
        P2SEL |= BIT2;
        break;
    case 3:
        TA0CCR0 = 2272;       // A4
        TA0CCR1 = 1136;
        P2SEL |= BIT2;
        break;
    case 4:
        TA0CCR0 = 1516;       // E5
        TA0CCR1 = 758;
        P2SEL |= BIT2;
        break;
    case 5:
        TA0CCR0 = 1136;       // A6
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
 * Function:  transmit
 * ----------------------
 * Transfers converted ADC data to Tx buffer and begins transmission
 */
void transmit()
{
    unsigned int i;
    for (i=0; i < sizeof(TxBuffer); i++)
    {
        TxBuffer[i] = Digits[i];
    }
    TxBufIndex = 0;
    IE2 |= UCA0TXIE;        // Enable USCI_A0 TX interrupt to begin UART transmission
}

// UART TX ISR to transmit data
#pragma vector = USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
    UCA0TXBUF = TxBuffer[TxBufIndex];
    TxBufIndex++;                           // Transmit next character

    if (TxBufIndex >= sizeof(TxBuffer)){    // Check if TX has been completed
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
        if (UCA0RXBUF == ','){
            RxBufIndex = 0;
            Flag = SAVE;
            IE2 &= ~UCA0RXIE;               // Disable USCI_A0 RX interrupt
        }
        else{
            RxBuffer[RxBufIndex] = UCA0RXBUF;
            RxBufIndex++;
        }
    }
    else{
        RxBufIndex = 0;
    }
}

/*
 * Function:  receive
 * ----------------------
 * Stores received UART data from buffer into an array
 */
void receive()
{
    unsigned int i;
    for(i=0; i < sizeof(Digits); i++)
    {
        Digits[i] = RxBuffer[i];
    }
}

/*
 * Function:  display
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
    keyVal = Digits[3];

    switch(keyVal) {

    case '1':
        P1OUT = BIT5;                   // Set digit position on
        displayDigit(Digits[0]);        // Display number
        __delay_cycles(4000);
        break;

    case '2':
        P1OUT = BIT5;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT4;
        displayDigit(Digits[1]);
        __delay_cycles(4000);
        break;

    case '3':
        P1OUT = BIT5;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT4;
        displayDigit(Digits[1]);
        __delay_cycles(4000);
        P1OUT = BIT7;
        displayDigit(Digits[2]);
        __delay_cycles(4000);
        break;

    case '4':
        P1OUT = BIT5;
        displayDigit(Digits[0]);
        __delay_cycles(2000);
        P1OUT = BIT4;
        displayDigit(Digits[1]);
        __delay_cycles(2000);
        P1OUT = BIT7;
        displayDigit(Digits[2]);
        __delay_cycles(2000);
        P1OUT = BIT6;
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
    switch(val) {                   // Use char to select display bit value
    case '0':
        P2OUT = ZERO;               // Set pins to display value
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
 *  ISR: Timer0 A0 Interrupt service routine
 * --------------------
 * if rising edge -> capture the timer register when pulse starts
 * else if falling edge -> capture the timer register when pulse ends
 */
#pragma vector = TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void)
{

    Edge = CCI;

    switch(TA1IV)
    {
    case TA1IV_NONE:
        break;
    case 10:                                        // Don't sample if overflowed
        break;
    case TA1IV_TACCR1:
        if (TA1CCTL1 & Edge){                       // start timer
            Start = TA1CCR1;
        }
        else{                                       // stop timer
            End = TA1CCR1;
            Travel_time = End - Start;              // Calculate the travel time
        }
        break;
    default:
        break;
    }
    TACTL &= ~CCIFG;                                // clear interrupt flag
}

/*
 * Function:    portInit0
 * ---------------------
 * Initializes microcontroller 0 registers and configures ADC
*/
void portInit0(void)
{
    WDTCTL = WDTPW | WDTHOLD;                       // stop watchdog timer
    DCOCTL = 0;                                     // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                          // Set DCO
    DCOCTL = CALDCO_1MHZ;

    // Configure UART //
    P1SEL = BIT2;                                   // P1.2=TXD
    P1SEL2 = BIT2;                                  // P1.2=TXD
    UCA0CTL1 |= UCSSEL_2;                           // SMCLK
    UCA0BR0 = 104;                                  // 1MHz 9600
    UCA0BR1 = 0;                                    // 1MHz 9600
    UCA0MCTL = UCBRS0;                              // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                           // **Initialize USCI state machine**

    // Set GPIO Pin //
    P1DIR |= BIT6;                                  // Set P1.6 as speaker driver
    P1SEL |= BIT6;

    // Configure PWM Timer //
    TACTL = TASSEL_2 + MC_1;                        // SMCLK, upmode
    TA0CCR0 = 12134;                                // PWM period
    TA0CCTL1 = OUTMOD_7;                            // CCR0 reset/set
    TA0CCR1 = 6067;                                 // PWM Duty Cycle

    // Configure Ultrasonic //
    P2DIR |= TRIG_P;                                  // Set P2.0 output as trigger
    P2OUT &= ~TRIG_P;                                 // Set P2.0 off
    P2DIR &= ~ECHO_P;                                 // Set P2.1 input as echo / 1A3_CCI1A
    P2SEL |= ECHO_P;
    P2REN |= ECHO_P;                                // Enable resistor
    P2OUT &= ~ECHO_P;                               // Pull-down mode

    // Configure Ultrasonic Timer //
    TA1CTL = MC_0;
    TA1CCTL1 |= SCS + CM_3 + CCIS_0 + CAP + CCIE;   // synchronous, rising/falling edge, compare input select, capture mode, interrupt enable
    TA1CTL |= TASSEL_2 + MC_2;

    __bis_SR_register(GIE);                         // interrupts enabled

}

/*
 * Function:    portInit1
 * ---------------------
 * Initializes microcontroller 1 registers and configures ADC
*/
void portInit1(void)
{
    WDTCTL = WDTPW | WDTHOLD;                       // stop watchdog timer
    DCOCTL = 0;                                     // Select lowest DCOx and MODx settings
    BCSCTL1 = CALBC1_1MHZ;                          // Set DCO
    DCOCTL = CALDCO_1MHZ;

    /* Configure UART */
    P1SEL = BIT1 + BIT2;                            // P1.1=RXD
    P1SEL2 = BIT1 + BIT2;                           // P1.1=RXD
    UCA0CTL1 |= UCSSEL_2;                           // SMCLK
    UCA0BR0 = 104;                                  // 1MHz 9600
    UCA0BR1 = 0;                                    // 1MHz 9600
    UCA0MCTL = UCBRS0;                              // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                           // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;                                // Enable USCI_A0 RX interrupt

    /* Configure GPIO */
    P1DIR |= (BIT6 + BIT7 + BIT4 + BIT5);           // set P1.2-P1.5 pins to output
    P2DIR |= 0x7F;                                  // set P2 pins to output
    P2OUT = 0x00;                                   // reset all P2 output pins to clear 7-seg
    P2SEL &= ~(BIT6 + BIT7);                        // turn off XIN to enable P2.6

    __bis_SR_register(GIE);                         // interrupts enabled
}

