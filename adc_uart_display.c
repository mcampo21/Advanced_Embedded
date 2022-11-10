#include <msp430.h> 
#include <stdio.h>
#include <stdlib.h>

/***************************************************************************
 * adc_uart_display.c
 * ECGR 5431: Lab 7
 * Date: 10/07/22
 * Group 14: Michael Campo, Jeremie Tuzizila
 *
 * This code will enable an ADC to read voltage from a potentiometer.
 * The sampled ADC value will be sent via UART between two MCUs.
 * A digit of the sampled value between 0-1023 will be shown on a 7-segment
 * display dependent on the read voltage value in its proper digit place.
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
unsigned int OldVal = 0;
unsigned int data[5];
unsigned int TxBufIndex = 0, RxBufIndex = 0;
static char TxBuffer[6], RxBuffer[5], Digits[6];
enum Flags {Stop, Sample, Save};
volatile enum Flags Flag = Stop;


/* Function Prototypes */
int hwFlag(void);
void portInit0(void);
void portInit1(void);
unsigned int sampleADC(void);
void convertADC(unsigned int);
void transmit(void);
void receive(void);
void display();
void displayDigit(char);

int main(void)
{
    int mcu = hwFlag();

    if (mcu == 0){                // Activate Sampling code on MCU0
        portInit0();

        while(1){
            if (Flag == Sample){                                // Activate function when timer changes flag
                unsigned int readVal = sampleADC();             // Sample ADC
                convertADC(readVal);                            // Convert ADC value into char
                transmit();                                     // Send converted char's through UART
                OldVal = readVal;                               // Store previous value
                Flag = Stop;
            }
        }
    }

    if (mcu == 1){              // Activate Display code on MCU1
        portInit1();

        while(1){
            if (Flag == Save){                                 // Update display value
                receive();                                     // Get value from UART Rx
                IE2 |= UCA0RXIE;                               // Enable USCI_A0 RX interrupt
                Flag = Stop;                                   // Don't update display value
            }
            else{
                display();
                __delay_cycles(4000);
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
    P1DIR &= ~BIT0;                 // set P1.0 to input
    P1REN = BIT0;                   // enable pull resistor
    P1OUT = 0;                      // set to pull-down

    return P1IN&0x01;

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

    /* Configure UART */
    P1SEL = BIT2 + BIT1;                            // P1.1=RXD / P1.2=TXD
    P1SEL2 = BIT2 + BIT1;                           // P1.1=RXD / P1.2=TXD
    UCA0CTL1 |= UCSSEL_2;                           // SMCLK
    UCA0BR0 = 104;                                  // 1MHz 9600
    UCA0BR1 = 0;                                    // 1MHz 9600
    UCA0MCTL = UCBRS0;                              // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                           // **Initialize USCI state machine**
    //IE2 |= UCA0TXIE;                                // Enable USCI_A0 TX interrupt

    /* Configure Timer */
    TACTL = TASSEL_2 + MC_1 + ID_3;                 // SMCLK, upmode, 1Mhz/4 = 125KHz
    TACCR0 = 12500-1;                               // Compare value, 10Hz = 125KHz / 12500
    TACCTL0 |= CCIE;                                 // CCR0 interrupt enabled

    /* Configure ADC Channel */
    P1SEL |= BIT5;                                  // set P1.5 to analog input
    ADC10CTL1 = INCH_5 + ADC10DIV_3;                // select single channel A5, ADC10CLK/3
    ADC10CTL0 = ADC10SHT_3;// + ADC10ON;         // sample/hold 64 cycle, multiple sample, turn on ADC10
    ADC10AE0 |= BIT5;                               // enable P1.5 for analog input

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
    P1SEL = BIT1 + BIT2;                                   // P1.1=RXD
    P1SEL2 = BIT1 + BIT2;                                  // P1.1=RXD
    UCA0CTL1 |= UCSSEL_2;                           // SMCLK
    UCA0BR0 = 104;                                  // 1MHz 9600
    UCA0BR1 = 0;                                    // 1MHz 9600
    UCA0MCTL = UCBRS0;                              // Modulation UCBRSx = 1
    UCA0CTL1 &= ~UCSWRST;                           // **Initialize USCI state machine**
    IE2 |= UCA0RXIE;                                // Enable USCI_A0 RX interrupt

    /* Configure GPIO */
    P1DIR |= (BIT6 + BIT3 + BIT4 + BIT5);           // set P1.2-P1.5 pins to output
    P2DIR |= 0x7F;                                  // set P2 pins to output
    P2OUT = 0x00;                                   // reset all P2 output pins to clear 7-seg
    P2SEL &= ~(BIT6 + BIT7);                        // turn off XIN to enable P2.6

    __bis_SR_register(GIE);                         // interrupts enabled
}

/*
 * Function:  sampleADC
 * ----------------------
 * Enables and starts ADC conversion.
 * Return sampled value from ADC10MEM Register.
 *
 * returns: int value between 0-1023
 */

unsigned int sampleADC(void)
{
    unsigned int sum = 0;
    unsigned int i = 0;
    ADC10CTL0 |= ADC10ON;
    for (i=0; i<32; i++){                               // sample the ADC value 32 times
        ADC10CTL0 |= ENC + ADC10SC;                     // enable and start conversion
        while ( (ADC10CTL1 & ADC10BUSY) == 0x01);       // wait until sample operation is complete
        sum += ADC10MEM;
        //__delay_cycles(10);
        }

    if (sum == 0){return 0;}
    unsigned int val = (sum/32);                     // take the average sampled value

    if (val == 0){return 0;}                           // fix bottom edge condition
    if (val >= 1015){return 1023;}                     // fix top edge condition created by sampled rounding

    int difference = abs(val-OldVal);                  // find the difference between the newly sampled value and previous value
    int thresh = 5;

    if (val > 700) thresh = 7;

    if (difference < thresh)
    {                                // if difference less than 2, return old value to help prevent oscillating
        return OldVal;
    }

    ADC10CTL0 &= ~ADC10ON;
    return val;
}

/*
 * Function:  convertADC
 * ----------------------
 * Receives the sampled ADC value and splits it
 * into digit places using modulo division.
 * Based off position place a key is created.
 * Values stored into global array.
 */

void convertADC(unsigned int readVal)
{
    unsigned int key=0;
    if (readVal > 999){                     // Capture 1000s place
        key = 4;
    }
    else if (readVal > 99){                 // Capture 100s place
        key = 3;
    }
    else if (readVal > 10){                 // Capture 10s place
        key = 2;
    }
    else {
        key = 1;
    }


    Digits[0] = (readVal % 10) + 48;        // Use modulo to get value from each numbers place
    readVal /= 10;                          // Add 48 to get correct ASCII format

    Digits[1] = (readVal % 10) + 48;
    readVal /= 10;

    Digits[2] = (readVal % 10) + 48;
    readVal /= 10;

    Digits[3] = (readVal % 10) + 48;

    Digits[4] = key + 48;
    Digits[5] = 44;
}

/*
 * Function:  transmit
 * ----------------------
 * Transfers converted ADC data to Tx buffer and begins transmission
 */
void transmit(){
    unsigned int i;
    for (i=0; i < sizeof(TxBuffer); i++)
    {
        TxBuffer[i] = Digits[i];
    }
    TxBufIndex = 0;
    IE2 |= UCA0TXIE;         // Enable USCI_A0 TX interrupt to begin transmission
}

/*
 * Function:  receive
 * ----------------------
 * Stores received UART data from buffer into an array
 */
void receive()
{
    unsigned int i;
    for (i=0; i < sizeof(Digits); i++)
    {
        Digits[i] = (unsigned int)RxBuffer[i];
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
    keyVal = Digits[4];

    switch(keyVal) {

    case '1':
        P1OUT = BIT6;                   // Set digit position on
        displayDigit(Digits[0]);        // Display number
        break;

    case '2':
        P1OUT = BIT6;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT3;
        displayDigit(Digits[1]);
        break;

    case '3':
        P1OUT = BIT6;
        displayDigit(Digits[0]);
        __delay_cycles(4000);
        P1OUT = BIT3;
        displayDigit(Digits[1]);
        __delay_cycles(4000);
        P1OUT = BIT4;
        displayDigit(Digits[2]);
        break;

    case '4':
        P1OUT = BIT6;
        displayDigit(Digits[0]);
        __delay_cycles(2000);
        P1OUT = BIT3;
        displayDigit(Digits[1]);
        __delay_cycles(2000);
        P1OUT = BIT4;
        displayDigit(Digits[2]);
        __delay_cycles(2000);
        P1OUT = BIT5;
        displayDigit(Digits[3]);
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


// Timer A0 interrupt service routine to control sampling occurrence
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A(void)
{
    Flag = Sample;
}

// UART Tx interrupt service routine to transmit data
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)              // transmitter ISR
{
    UCA0TXBUF = TxBuffer[TxBufIndex];                   // TX next character
    TxBufIndex++;

    if (TxBufIndex >= sizeof(TxBuffer)){            // TX over?
        TxBufIndex = 0;
        IE2 &= ~UCA0TXIE;                               // Disable USCI_A0 TX interrupt
    }
}

// UART Rx interrupt service routine to control receive data
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)      // Interrupt to receive data on MCU1
{
    if (RxBufIndex < 6)
    {
        if (UCA0RXBUF == ',')      // if ',' has been received save the 4-Digits
        {
            RxBufIndex = 0;         // reset the Rx index
            Flag = Save;
            IE2 &= ~UCA0RXIE;                               // Disable USCI_A0 RX interrupt
        }
        else
        {
            RxBuffer[RxBufIndex] = UCA0RXBUF;       //
            RxBufIndex++;
        }
    }
    else{
        RxBufIndex = 0;
    }

}
