/*****************************************************************************
 Add suitable comments to describe this code
 This header should explain what the program does,
 and what resources are required of the cpu and I/O pins.
 *****************************************************************************/

//Standard MSP430 includes
#include <msp430.h>
//Each program should be configured with each of the following sections
// clearly delineated as shown below.

// ***************** Constant Definitions Section ***************************************
//All constants should be in upper-case letters only.
#define MAKEDEBOUNCETIME  5		//Definitions for debounce Times (portA and portB)
#define TIMER_INTERVAL   125    //definition for interupt time interval(125 = 1ms for 1MHz with 8 divider)
#define NUMBER_OF_INPUTS 3		//definition for number of input
// ***************** End of Constant Definitions ****************************************

//****************** Section for Compiler pre-processor defines/Substitutions ***********
//This is an example :
#define BUTTON      BIT3                    //macro for the button(P1OUT)
#define REDLED      BIT0					//macro for the red led(P1OUT)
#define GREENLED    BIT6					//macro for the green led(P1OUT)
#define PLAYER1P1   BIT3				//macro for player1 port 1(P1OUT)
#define PLAYER1P2   BIT4				//macro for player1 port 2(P1OUT)
#define PLAYER1P3   BIT5				//macro for player1 port 3(P1OUT)
#define PLAYER2P1   BIT0				//macro for player1 port 1(P2OUT)
#define PLAYER2P2   BIT1				//macro for player1 port 2(P2OUT)
#define PLAYER2P3   BIT2				//macro for player1 port 3(P2OUT)

//boolean defines
#define DEF_TRUE    1
#define DEF_FALSE   0


// You should include similar defines for the other LED and debug pin operations
//******************* End of pre-processor defines section *******************************

//*******************Type Definitions Section*********************************************
typedef enum {
	DbExpectHigh, DbValidateHigh, DbExpectLow, DbValidateLow
} DbState;
typedef enum {
	Off, On
} SwitchStatus;
typedef enum{
	P1,P2
}PortType;
typedef struct {
	DbState ControlState;               //current state of the button
	SwitchStatus CurrentValidState;     //current state of button(off or on)
	unsigned int snapshot;              //stores next time to be checked against
	PortType	 Port;                  //Port used (P1 or P2)
	int			 Pin;                   //Pin used (ex. BIT3)

} SwitchDefine;

typedef struct{
	SwitchDefine * inputs; //array of inputs(buttons)
	char * sequence;       //array of integers representing sequences(not being used right now)
}PlayerDefine;
typedef enum{
	NoInput,
	GameOver,
	Correct,
	Finished
}TurnResult; //enum to hold result of a turn taken(NoInput means that turn is still waiting for a press
char sequenceLength;     //size of the current sequence
char sequenceIndex;      //place in the current sequence
char sequence[100];      //array that holds a sequence up to 100
char readyForNextTurn = DEF_FALSE;   //boolean that keeps track of whether turn can be taken(buttons have been released)

//******************End of Type Definitions Section***************************************

//********************** Global Variables Declaration Section ****************************
//The names of all global variables should begin with a lower case g
PlayerDefine player1;        //variable holding player 1 information
PlayerDefine player2;        //variable holding player 2 information
SwitchDefine p1inputs[NUMBER_OF_INPUTS];
SwitchDefine p2inputs[NUMBER_OF_INPUTS];
unsigned int g1mSTimeout;    //This variable is incremented by the interrupt handler and
// decremented by a software call in the main loop.
unsigned int tenSecondTimer; //This variable increments every 10 seconds
unsigned int halfSecondTimer; //This variable increments every half second
unsigned char playerTurn;            //number of clicks of the Rotary Encoder

//********************** End of Global Variables Declaration Section *********************

//********************** Function Prototypes *********************************************
//This function returns the instantaneous value of the selected switch
SwitchStatus GetSwitch(SwitchDefine *Switch);

//This function debounces a switch input
DbState Debouncer(SwitchDefine *Switch);

//Initialize all variables
void InitializeVariables(void);

// Initialize all hardware subsystems
void InitializeHardware(void);
//These are explicit called from InitializeHardware()
void InitTimerSystem(); //This should set up a periodic interrupt at a 1 mS rate using SMCLK as the clock source.
void InitPorts(); //This should also be called from ManageSoftwareTimers every 10 seconds

//This should deal with software initiated timers.
void ManageSoftwareTimers(void);

//method used for taking a turn
TurnResult takeTurn(PlayerDefine * player);

//verify ready for next turn
char verifyReady(PlayerDefine * player);

//stop the watchdog timer helper function
void stopWatchDog();
//********************** End of Function Prototypes Section ******************************

// ======== main ========
void main(void) {
	//stop the watchdog
	stopWatchDog();
//*************** Initialization Section ***************************
	InitializeVariables();
	InitializeHardware();
// ********************* End of Initialization Section *********************
	//Enable Global Interrupts Here ONLY
	//set GIE bit in status register for timer interupts
	_BIS_SR(GIE);
	unsigned int last_value = g1mSTimeout;
	int i = 0; //loop variable
	while (1) {
		if(g1mSTimeout != last_value){
		ManageSoftwareTimers();
		//debounce inputs for the current player
		switch(playerTurn){
		case 0://game over wait for new game start
			//start game when button is pressed
			if(P1IN & BUTTON){
				playerTurn = 1;
			}
			break;
		case 1: //player 1's turn
			//debounce player 1's inputs
			for(i = 0;i<NUMBER_OF_INPUTS;i++){
				Debouncer(&player1.inputs[i]);
			}
			//verify ready for turn
			if(readyForNextTurn){
				switch(takeTurn(&player1)){
					case NoInput:
					case Correct:
						//do nothing(all actions handled by takeTurn() method
						break;
					case GameOver:
						//set playerTurn to 0
						playerTurn = 0;
						break;
					case Finished:
						//set playerTurn to 2
						playerTurn = 2;
						break;
				}
			}
			else{
				//see if its ready now
				readyForNextTurn = verifyReady(&player1);
			}
			break;
		case 2: //player 2's turn
			//debounce player 2's inputs
			for(i = 0;i<NUMBER_OF_INPUTS;i++){
				Debouncer(&player2.inputs[i]);
			}
			//verify ready for turn
			if(readyForNextTurn){
				switch(takeTurn(&player2)){
					case NoInput:
					case Correct:
						//do nothing(all actions handled by takeTurn() method
						break;
					case GameOver:
						//set playerTurn to 0
						playerTurn = 0;
						break;
					case Finished:
						//set playerTurn to 2
						playerTurn = 2;
						break;
				}
			}
			else{
				//see if its ready now
				readyForNextTurn = verifyReady(&player2);
			}
			break;
		}
	}
}
}
//This function returns the instantaneous value of the selected switch
SwitchStatus GetSwitch(SwitchDefine *Switch){
	SwitchStatus status = On;
	if(Switch->Port == P1){
		if(P1IN & (Switch->Pin)){
			//change status to off if bit is high(active low)
			status = Off;
		}
	}
	else{
		if(P2IN & (Switch->Pin)){
			//change status to off if bit is high(active low)
			status = Off;
		}
	}
	return status;
}

//Function that debounces
DbState Debouncer(SwitchDefine *Switch) {
//Code must be added to access the internal variable through the SwitchDefine pointer
	SwitchStatus currentReading = GetSwitch(Switch);
	switch (Switch->ControlState) {
		case DbExpectHigh:
			Switch->CurrentValidState = Off;
			if(currentReading == On){
				Switch->ControlState = DbValidateHigh;
				//take snapshot (snapshot will hold value to check against)
				Switch->snapshot = g1mSTimeout + MAKEDEBOUNCETIME;
				//check for rollover
				if(Switch->snapshot > 500){
					Switch->snapshot -= 500;
				}
			}
		break ;
		case DbValidateHigh:
			if(currentReading == On){
				//check to see if state has been high long enough
				if(g1mSTimeout == Switch->snapshot){
					//move to next state
					Switch->ControlState = DbExpectLow;
					//increment the press counter
					//toggle the red led
					P1OUT ^= REDLED;
					Switch->CurrentValidState = On;
				}
			}
			else{
				//low, move back to expecting high
				Switch->ControlState = DbExpectHigh;
			}
		break ;
		case DbExpectLow:
			Switch->CurrentValidState = On;
			if(currentReading == Off){
				Switch->ControlState = DbValidateLow;
				//take snapshot (snapshot will hold value to check against)
				Switch->snapshot = g1mSTimeout + MAKEDEBOUNCETIME;
				//check for rollover
				if(Switch->snapshot > 500){
					Switch->snapshot -= 500;
				}
			}
		break ;
		case DbValidateLow:
			if(currentReading == Off){
				//check to see if state has been high long enough
				if(g1mSTimeout == Switch->snapshot){
					//move to next state
					Switch->ControlState = DbExpectHigh;
					Switch->CurrentValidState = Off;
				}
			}
			else{
				//low, move back to expecting high
				Switch->ControlState = DbExpectLow;
			}
		break ;
		default: Switch->ControlState = DbExpectHigh ;
	}
//The internal state should be updated here. It should also be returned as a debugging aid....
	return Switch->ControlState ;
}

//deals with software updated timers
void ManageSoftwareTimers(void){
	//check if 500 ms has passed
	if(g1mSTimeout == 500){
		halfSecondTimer++;
		g1mSTimeout = 0;
		//toggle green LED
		P1OUT ^= GREENLED;
	}
	//check if 10 s has passed
	if(halfSecondTimer == 20){
		halfSecondTimer = 0;
		tenSecondTimer++;
		//reinitialile port direction
		InitPorts();
	}
}
void InitializeVariables(void) {
	playerTurn = 0;        //current turn
	sequenceLength = 0;    //current length of the sequence (empty at start)
	sequenceIndex = 0;     //current location in the sequence(start at the beginning)

	//initialize player 1 variables
	//allocate space
	player1.inputs = p1inputs;
	int i = 0;
	for(i = 0; i<NUMBER_OF_INPUTS;i++){
		player1.inputs[i].ControlState = DbExpectHigh;
		player1.inputs[i].CurrentValidState = Off;
		player1.inputs[i].Port = P1;
	}
	//set pins individually for each input
	player1.inputs[i].Pin = PLAYER1P1;
	player1.inputs[i].Pin = PLAYER1P2;
	player1.inputs[i].Pin = PLAYER1P3;

	//initialize player 2 variables
	//allocate space
	player2.inputs = p2inputs;
	for(i = 0; i<NUMBER_OF_INPUTS;i++){
		player2.inputs[i].ControlState = DbExpectHigh;
		player2.inputs[i].CurrentValidState = Off;
		player2.inputs[i].Port = P2;
	}
	//set pins individually for each input
	player2.inputs[i].Pin = PLAYER2P1;
	player2.inputs[i].Pin = PLAYER2P2;
	player2.inputs[i].Pin = PLAYER2P3;
}

// Initialize all hardware subsystems
void InitializeHardware(void){
	InitPorts();
	InitTimerSystem();
}

//sets up a periodic interrupt at a 1 mS rate using SMCLK as the clock source.
void InitTimerSystem(){ //
	//set the frequency of SMCLK
	DCOCTL = CALDCO_1MHZ;
	BCSCTL1 = CALBC1_1MHZ;
	//set value to count up to generate regular interupts
	TACCR0 = TIMER_INTERVAL;
	//enable capture/compare mode, no capture
	TACCTL0 = CCIE | CM_0;
	//set timer to desired mode (SMCLK, 8 divider, Up mode, timer clear enabled)
	TACTL = (TASSEL_2 | ID_3 | MC_1 | TACLR);
}

//intitializes the ports on startup
//This should also be called from ManageSoftwareTimers every 10 seconds
void InitPorts(){
	//initialize button

	//set output bits to zero to avoid false output
	P1OUT &= ~(REDLED + GREENLED);
	//set ports to output mode for leds
	P1DIR |= (REDLED + GREENLED);
	//set button to input
	P1DIR &= ~BUTTON;
	//set to pullup
	P1REN |= BUTTON;

	//initialize ports for player1
	int i = 0;
	for(i = 0;i<NUMBER_OF_INPUTS;i++){
		P1OUT &=~ player1.inputs[i].Pin;
		P2OUT &=~ player2.inputs[i].Pin;
	}

}
#pragma vector = TIMER0_A0_VECTOR
//interupt that is triggered when the timer reaches the specified interval
__interrupt void TimerA0_routine(void){
	//increment the global variable by 1ms
	g1mSTimeout++;
}

void stopWatchDog(){
	WDTCTL = WDTPW | WDTHOLD;
}

//method that processes a turn for a given player
//turns are taken until control is switched to the other player
TurnResult takeTurn(PlayerDefine * player){
	//check all inputs
	//if multiple buttons are being pressed the first On button is accepted
	char i = 0;
	char current = NUMBER_OF_INPUTS; //invalid current input(if changed a button is pressed)
	for(i = 0;i<NUMBER_OF_INPUTS;i++){
		if(player->inputs[i].CurrentValidState == On){
			current = i;
			break;
		}
	}
	//if no button pressed return NoInput
	if(current == NUMBER_OF_INPUTS){
		return NoInput;
	}
	//check against the sequence
	//check to see if sequence is over
	if(sequenceIndex == sequenceLength){
		//increment sequence size by one
		sequenceLength++;
		//set sequence Index to current input
		sequence[sequenceIndex] = current;
		//turn off turn taking to wait for buttons to not be pressed
		readyForNextTurn = DEF_FALSE;
		return Finished;
	}
	if(sequence[sequenceIndex] == current){
		//correct sequence
		//increment sequence index
		sequenceIndex++;
		//turn off turn taking to wait for buttons to not be pressed
		readyForNextTurn = DEF_FALSE;
		return Correct;
	}
	else{
		//wrong element returned
		//reset turn readiness
		readyForNextTurn = DEF_FALSE;
		return GameOver;
	}
}
//check all inputs to verify no buttons are being pressed
//used to space out input phases to human level
char verifyReady(PlayerDefine * player){
	int i = 0;
	for(i = 0;i<NUMBER_OF_INPUTS;i++){
		if(player->inputs[i].CurrentValidState == On)
			return DEF_FALSE;
	}
	return DEF_TRUE;
}
