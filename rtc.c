/* rtc.c - Functions to control rtc(real time clock) in kernel
*/

#include "rtc.h"
#include "lib.h"
#include "i8259.h"

/*
 * rtc_handler
 *   DESCRIPTION: an interrupt handler specialized with dealing rtc interrupts.
 *   INPUTS: i = interrupt vector
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: Handle rtc interrupt, send end-of-interrupt 
 *				   to unmask handled interrupt 
 *			 	
 */   

void rtc_handler(int i)
{	
	// call the test cases
	//test_interrupts();
	// calls register C, which is responsible for handling rtc
	outb(RTC_REGC, RTC_CMD_PORT);
	
	inb(RTC_DATA_PORT);
	
	int32_t k;
            for(k=0;k<MAX_NUMB_TERMINAL;k++){
			 if(rtcreadcalled[k]==1) //Check to see if RTC read has been called.
			 {	
				
			 	RTCreadCheck[k]=1;//RTC int has been called. Now exit the read loop
			
			 }    
			}
			
	// send end-of-interrupt signal
	send_eoi(RTC_IRQ); 


}
/*
 * rtc_init
 *   DESCRIPTION: initialize rtc device to avoid getting an undefined state,
 *				  since it is never initialized by BIOS and backed up in a battery.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: enables rtc_handler to use rtc
 *                 sets the frquency of rtc
 */   
void rtc_init()
{
	
	// local variables for each register
	uint8_t temp;
	uint8_t temp2;
	int i;
	// calls register A, which is responsible for handling frequency
	outb(RTC_REGA, RTC_CMD_PORT);
	temp = inb(RTC_DATA_PORT);
	outb(RTC_REGA, RTC_CMD_PORT);
	// select the frequency rate : range from 2Hz to 8.192kHz
	outb(FREQ3 | temp, RTC_DATA_PORT);
	
	// calls register B, which is responsible for enabling interrupt
	outb(RTC_REGB, RTC_CMD_PORT); 
	temp2 = inb(RTC_DATA_PORT);
	outb(RTC_REGB, RTC_CMD_PORT);
	// set the bit 6 as 1 to enable PIE(Periodic Interrupt Enable).
	outb((RTC_PIE | temp2), RTC_DATA_PORT);

	for(i=0;i<MAX_NUMB_TERMINAL;i++)
	{
	RTCreadCheck[i] = 0;
	}
}
/*
 *   rtc_open
 *   DESCRIPTION: rtc open set the frequency to 2hz, when default opened
 *   INPUTS: dummy -- Dummy Variable to Align Args
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none 
 *			 	
 */   
int32_t rtc_open(uint32_t dummy)
{	
	uint8_t temp;
	// calls register A, which is responsible for handling frequency
	outb(RTC_REGA, RTC_CMD_PORT);
	temp = inb(RTC_DATA_PORT);
	outb(RTC_REGA, RTC_CMD_PORT);
	// select the frequency rate : range from 2Hz to 8.192kHz
	outb(FREQF | temp, RTC_DATA_PORT); 
	//setting the frequency to 2 hz
	return 0;

}

/*
 *   rtc_read
 *   DESCRIPTION: RTC read wait's until the rtc interrupt occurs. If it does occur, it returns 0
 *   else it wait until RTC interrupt occurs
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 for letting caller know RTC occured
 *   SIDE EFFECTS: none 
 *			 	
 */   
int32_t rtc_read()
{	
	int32_t cur_term = get_current_terminal();
	if(RTCreadCheck[cur_term] != READ_ON)//Make sure previous read has been completed.
		rtcreadcalled[cur_term] = READ_ON;

	while(RTCreadCheck[cur_term] != READ_ON) {} //wait until RTC handler set the RTCreadCheck to READ_ON
	
    rtcreadcalled[cur_term] = READ_OFF; //Put it back to init flag
	RTCreadCheck[cur_term] = READ_OFF;

	return 0; 	//Since the RTC interrupt has occurred, return 0 with success

}

/*
 *   rtc_write
 *   DESCRIPTION: RTC takes in the frequency to set and sets it to the given frequency. 
 *	 it uses the frequency converter helper array which contains  (i=1)2hz (i=2)4hz (i=3)8hz..
 *   accordingly and uses the i index to find the register bits for Register A
 *   INPUTS: dummy -- Dummy Variable to Align Args
 *			 dummy1 -- Dummy Variable to Align Args
 *           freq_ptr -- Pointer to Frequency to Set (1~15) 
 * 			 dummy2 -- Dummy Variable to Align Args
 *   OUTPUTS: writes the frequency to RTC
 *   RETURN VALUE: return 0 on success else -1 on failure
 *   SIDE EFFECTS: none 
 *			 	
 */ 
int32_t rtc_write(uint32_t dummy, uint32_t dummy1, int32_t* freq_ptr, uint32_t dummy2)
{
	int32_t freq = *freq_ptr; //Frequency value pointer

	char Fs=0; //Frequency to set
	uint8_t temp;
	int FrequencySet[NUM_FREQ];
	int i;
	int given_freq_check=0;//2,4,8,16..hz to put in array
	int SetFrequency= WRONGFREQ; //variable for checking if we have found a corresponding freq.If never found, it is 20.
	int tempNumber=0;
	if(freq>MAXFREQ) //Out of Range for frequency.
	{
		return -1;
	}

	
	for(i = 0;i < NUM_FREQ;i++)	//Setting Register bits to Array for compare
	{							//In Array (i=1)2hz (i=2)4hz (i=3)8hz....
		if(i==1)
		{
			given_freq_check=FIRST_FREQ; //First 2hz, to skip out from 0
		}
		
		given_freq_check=given_freq_check*2;    
		
		FrequencySet[i]=given_freq_check;
	}
	
	for(i=0;i< NUM_FREQ;i++)	//Finding the right Register bit for input Frequency
	{
	
		tempNumber=FrequencySet[i];
		
		if(tempNumber==freq)
		{
			SetFrequency=i;//15-Index gives the register bit for the Frequency
		}
	
	}
	
	if(SetFrequency==WRONGFREQ) //Wrong index. Never found the correct frequency in range
	{
		return -1;
	}
	
	SetFrequency= MAX_SELECT_BIT - SetFrequency; //Calculating Select Bits for Register A based on Frequency array index
	
	
		Fs=(SetFrequency&CBIT1); // calculate the frequency with last 4 bit, with clearing unrelated bits.
		// calls register A, which is responsible for handling frequency
		outb(RTC_REGA, RTC_CMD_PORT);
		temp = inb(RTC_DATA_PORT);
		outb(RTC_REGA, RTC_CMD_PORT);
		// select the frequency rate : range from 2Hz to 8.192kHz
		outb(Fs | (temp & CBIT2), RTC_DATA_PORT);
		//setting the frequency
		return 0;
}


/*
 *   rtc_close
 *   DESCRIPTION: RTC close is a function that that returns success with return value 0
 *   INPUTS: None
 *   OUTPUTS: None
 *   RETURN VALUE: 0 when called
 *   SIDE EFFECTS: none 
 *			 	
 */ 
int32_t rtc_close()
{
	return 0; //return 0 with success


}


/*
 *   rtc_test
 *   DESCRIPTION: Testing functionality for rtc.
 *   INPUTS: None
 *   OUTPUTS: Test all rtc functionality implemented
 *   RETURN VALUE: None
 *   SIDE EFFECTS: None 
 *			 	
 */ 
void rtc_test()
{

	reset_screen(); //clear the screen for testing
	rtc_open(10); //Call rtc open. set rtc to 2hz
	int32_t* ptr;
	*ptr = 64;
	rtc_write(0,0,ptr,0); //write rtc freq
	while(1)	//wait until rtc_read respond for having rtc interrupt
	{
	
	rtc_read();
	printf("hihi"); //RTC read successful. Print hi for showing freq rate
	
	}
}

