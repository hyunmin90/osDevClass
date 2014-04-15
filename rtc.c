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
	RTCreadCheck = 0;
		
}

/*
 *   rtc_open
 *   DESCRIPTION: rtc open set the frequency to 2hz, when default opened
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none 
 *			 	
 */   


void rtc_open()
{
	uint8_t temp;
	// calls register A, which is responsible for handling frequency
	outb(RTC_REGA, RTC_CMD_PORT);
	temp = inb(RTC_DATA_PORT);
	outb(RTC_REGA, RTC_CMD_PORT);
	// select the frequency rate : range from 2Hz to 8.192kHz
	outb(FREQF | temp, RTC_DATA_PORT); 
	//setting the frequency to 2 hz

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

int rtc_read()
{
	while(RTCreadCheck!=1) //wait until RTC handler set the RTCreadCheck to 1
	{
	
	}
    
	RTCreadCheck = 0;
	return 0; 	//Since the RTC interrupt has occured, return 0 with success


}

/*
 *   rtc_write
 *   DESCRIPTION: RTC takes in the frequency to set and sets it to the given frequency
 *   INPUTS: Frequency to set to. 1~15 is the range of possible set.
 *   OUTPUTS: writes the frequency to RTC
 *   RETURN VALUE: return 0 on success else -1 on failure
 *   SIDE EFFECTS: none 
 *			 	
 */ 
int rtc_write(int32_t frequencyToSet)
{
	char Fs=0; //Frequency to set
	uint8_t temp;
	char FrequencySet[11];
	int i;
	int powerofTwo=0;
	int SetFrequency=20; //variable for checking if we have found a corresponding freq.If never found, it is 20.
	
	if(frequencyToSet>MAXFREQ) //Out of Range for frequency.
	{
		return -1;
	}
	
	for(i=0;i<11;i++)	//Setting Register bits to Array for compare
	{
	powerofTwo=i*2;
        
	FrequencySet[i]=powerofTwo;
	}
	
	for(i=0;i<11;i++)	//Finding the right Register bit for input Frequency
	{
		if(FrequencySet[i]==frequencyToSet)
		{
			SetFrequency=i;//15-Index gives the register bit for the Frequency
		}
	
	}
	
	if(SetFrequency==WRONGFREQ) //Never found the correct frequency in range
	{
		return -1;
	}
	
	SetFrequency=15-SetFrequency+1; //Calculating the Register bit, out of 15
	
	
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
int rtc_close()
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
	rtc_open(); //Call rtc open. set rtc to 2hz
	rtc_write(16); //write rtc freq
	while(1)	//wait until rtc_read respond for having rtc interrupt
	{
	
	rtc_read();
	printf("hihi"); //RTC read successful. Print hi for showing freq rate
	
	}



}
