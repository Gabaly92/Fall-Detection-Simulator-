#include <avr/io.h>
#include <avr/interrupt.h>
#include "bit.h"
#include "lcd.h"
#include "keypad.h"

/* SpiPinDefinitions */
#define DDR_SPI		DDRB
#define DD_SS	    DDB4  // Active Low
#define DD_MOSI		DDB5
#define DD_MISO		DDB6
#define DD_SCK	    DDB7
#define PORT_SPI	PORTB

/* SlaveInititalization Function */
void SPI_SlaveInit(void)
{
	/* MISO = Output / MOSI & SS & SCK = Input */
	DDR_SPI |= (1<<DD_MISO) & ~(1<<DD_SCK) & ~(1<<DD_SS) & ~(1<<DD_MOSI);
	/* Enable SPI  and SPI Interrupt*/
	SPCR = (1 << SPE)|(1<<SPIE);
	/* Enable Global Interrupt */
	SREG |= 0x80;
}

typedef struct task {
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} task;
task tasks[3];

const unsigned char tasksNum = 3;
const unsigned long tasksPeriodGCD = 1;
const unsigned long Period_LCD = 100;
const unsigned long Period_Morse = 1;
const unsigned long Period_DataCheck = 1;

int TickFct_LCD(int state);
int TickFct_Morse(int state);
int TickFct_DataCheck(int state);

void TimerISR() {
	unsigned char i;
	for (i = 0; i < tasksNum; ++i) { // Heart of the scheduler code
		if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriodGCD;
	}
}
void TimerOn() {
	TCCR1B = (1 << WGM12) | (1 << CS10) | (1 << CS11); // (ClearTimerOnCompare mode). // Prescaler=?
	TIMSK1 = (1 << OCIE1A); // Enables compare match interrupt
	SREG |= 0x80; // Enable global interrupts
}
void TimerSet(int ms) {
	TCNT1 = 0; // Clear the timer counter
	OCR1A = ms * 125; // Set the match compare value
}
ISR(TIMER1_COMPA_vect) {
	//RIOS
	TimerISR();
}

//Variables
unsigned char fall;
unsigned char help;
// LCD Task
enum LCDStates {LCDWaitFall,LCDDetectedFall,Emergency};
int TickFct_LCD( int state )
{
	const unsigned char lcdWait[] = {'S','t','a','b','l','e','\0'};
	const unsigned char lcdDetect[] = {'F','a','L','L',' ','D','e','t','e','c','t','e','d','\0'};
	const unsigned char lcdhelp[] = {'C','a','l','l',' ','P','o','l','i','c','e','\0'};
	// Transitions
	switch(state) {
		case -1:
		state = LCDWaitFall;
		break;
		
		case LCDWaitFall:
		if(fall == 1)
		state = LCDDetectedFall;
		break;
		
		case LCDDetectedFall:
		if(help == 1)
		{
			state = Emergency;
		}
		break;
		
		case Emergency:
		state = Emergency;
		break;
		
		default:
		state = -1;
		break;
	}
	// Actions
	switch(state) {
		case -1:
		break;
		
		case LCDWaitFall:
		LCD_DisplayString(0x01, lcdWait);
		break;
		
		case LCDDetectedFall:
		LCD_DisplayString(0x01, lcdDetect);
		break;
		
		case Emergency:
		LCD_DisplayString(0x01, lcdhelp);
		break;
		
		default:
		break;
	}
	return state;
}

// Morse Task
// Variables
int i = 0;
int j = 0;
enum MorseStates {DotOn,DotOff,DotsSpace,SOSpace,OSSpace,DashON,DashOff,DashSpace};
int TickFct_Morse( int state )
{
	// Transitions
	switch(state)
	{
		case -1:
		state = DotOn;
		break;
		
		case DotOn:
		if(i >= 500)
		{
			i = 0;
			j++;
			state = DotsSpace;
		}
		else
		{
			state = DotOff;
		}
		break;
		
		case DotOff:
		state = DotOn;
		break;
		
		case DotsSpace:
		if(j == 3)
		{
			i = 0;
			j = 0;
			state = SOSpace;
		}
		else if(i >= 500)
		{
			i = 0;
			state = DotOn;
		}
		else
		{
			state = DotsSpace;
		}
		break;
		
		case SOSpace:
		if(i >= 1500)
		{
			i = 0;
			state = DashON;
		}
		else
		{
			state = SOSpace;
		}
		break;
		
		// dash
		case DashON:
		if(i >= 1500)
		{
			i = 0; j++;
			state = DashSpace;
		}
		else
		{
			state = DashOff;
		}
		break;
		
		case DashOff:
		state = DashON;
		break;
		
		case DashSpace:
		if(j == 3)
		{
			i = 0;
			j = 0;
			state = OSSpace;
		}
		else if(i >= 500)
		{
			i = 0;
			state = DashON;
		}
		else
		{
			state = DashSpace;
		}
		break;
		
		case OSSpace:
		if(i >= 1500)
		{
			i = 0;
			state = DotOn;
		}
		else
		{
			state = OSSpace;
		}
		break;
		
		default:
		state = -1;
		break;
	}
	// Actions
	switch(state) {
		case -1:
		break;
		
		case DotOn:
		PORTC = SetBit(PORTC,0,1);
		i++;
		break;
		
		case DotOff:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		case DotsSpace:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		case SOSpace:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		//dash
		case DashON:
		PORTC = SetBit(PORTC,0,1);
		i++;
		break;
		
		case DashOff:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		case DashSpace:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		case OSSpace:
		PORTC = SetBit(PORTC,0,0);
		i++;
		break;
		
		default:
		break;
	}
	return state;
}

int cnt;
enum DataCheckStates {WaitFall,LEDOn,waitbuttonpress,callhelp};
int TickFct_DataCheck(int state)
{
	// Transitions
	switch(state)
	{
		case -1:
		state = WaitFall;
		break;
		case WaitFall:
		if( GetBit(PINC,1) == 0x00 )
		{
			state = LEDOn;
		}
		else{state = WaitFall;}
		break;
		
		case LEDOn:
		state = waitbuttonpress;
		break;
		
		case waitbuttonpress:
		// button not pressed within 10 seconds
		if( (cnt >= 10000) && GetBit(PINC,2) )
		{
			state = callhelp;
		}
		else
		{
			state = waitbuttonpress;
		}
		break;
		
		case callhelp:
		state = callhelp;
		break;
		
		default:
		state = -1;
		break;
	}
	// Actions
	switch(state)
	{
		case WaitFall:
		PORTC = SetBit(PORTC,6,0);
		fall = 0;
		break;
		
		case LEDOn:
		PORTC = SetBit(PORTC,6,1);
		fall = 1;
		break;
		
		case waitbuttonpress:
		cnt++;
		break;
		
		case callhelp:
		help = 1;
		break;
		
		default:
		break;
	}
	return state;
}

// Main
int main (void)
{
	DDRD = 0xFF; PORTD = 0x00; // output - LCD data
	DDRA = 0xFF; PORTA = 0x00; // output - 0,1 LCD control bits
	DDRC = 0xF9; PORTC = 0x06; // output - Speaker
	LCD_init(); // Initialize LCD screen
	unsigned char k = 0;
	tasks[k].state = -1;
	tasks[k].period = Period_LCD;
	tasks[k].elapsedTime = tasks[0].period;
	tasks[k].TickFct = &TickFct_LCD;
	k++;
	tasks[k].state = -1;
	tasks[k].period = Period_Morse;
	tasks[k].elapsedTime = tasks[0].period;
	tasks[k].TickFct = &TickFct_Morse;
	k++;
	tasks[k].state = -1;
	tasks[k].period = Period_DataCheck;
	tasks[k].elapsedTime = tasks[0].period;
	tasks[k].TickFct = &TickFct_DataCheck;

	TimerSet(tasksPeriodGCD); // value set should be GCD of all tasks
	TimerOn();

	while(1)
	{
	}
}