#include "scheduler.h"
#include "pcb.h"
#include "debug.h"
#include "lib.h"
#include "i8259.h"

extern pcb_t* top_process[NUM_TERMINALS];
extern int32_t num_progs[NUM_TERMINALS];

/*
 *   pit_handler
 *   DESCRIPTION: Pit handler is being called when IRQ0 is raised (0x20)
 *   Every time IRQ0 is raised (0x20,with 50HZ), the pit handler calls the switch_task 
 *   which takes input of task number to switch to. 
 *   INPUTS: NONE
 *   OUTPUTS: None
 *   SIDE EFFECTS: Switch to next task every 50HZ 
 */   
void pit_handler(){
	int32_t next_task_num = get_next_task_number(); //Gets the next task number
	if(next_task_num == -1){  //Check if there is no task to switch
		LOG("No Next Task!\n");
		send_eoi(PIT_IRQ);	//Send eoi to tell interrupt is dealt
		return;
	}
	
	switch_task(next_task_num); //Switch to next task

}


/*
 *   switch_task 
 *   DESCRIPTION: Works to switch Task.It takes in the task number from the queue to pick
 *	 and switch to that particular process. while so it flushes its tlb and save it's 
 *   currently running process registers to the pcb_ptr.
 *   INPUTS: new_task_num- This is the new task number to switch to
 *   OUTPUTS: None
 *   SIDE EFFECTS: Switch the task to the task with input Task num 
 */   


void switch_task(int32_t new_task_num)
{
	pcb_t* pcb_ptr = get_pcb_ptr();  //Get the current pcb
	pcb_t* top_pcb = top_process[new_task_num]; //Gets the top process to switch to 

	set_cr3_reg(top_pcb -> pg_dir); //Flush the TLB to process to switch to

	pcb_ptr -> esp0 = tss.esp0;  //Saves the ESP0 of current process
 	asm volatile("movl %%esp, %0;"   // Saving the esp, and ebp of current process
 		"movl %%ebp, %1;" : 
 		"=b" (pcb_ptr -> esp),
 		"=c" (pcb_ptr -> ebp));

 	tss.esp0 = top_pcb -> esp0; //Updating TSS's esp0 to switching task's esp0
	asm volatile("movl %0, %%esp;" //Updating esp,ebp to switching task's
		"movl %1, %%ebp;":: 
		"b" (top_pcb -> esp),
		"c" (top_pcb -> ebp)); 
	send_eoi(PIT_IRQ);	//Send end of interrupt
	asm volatile("leave; ret;"); //Return to handler
}

/*
 * pit_init
 *   DESCRIPTION: Initialize the pit with the desired mode and channel with
 *				  Frequency we desire 
 *   INPUTS: Channel-Channel to choose for PIT
 *			 mode -Mode to Choose in PIT
 *			     0 0 0 = Mode 0 (interrupt on terminal count)
 *               0 0 1 = Mode 1 (hardware re-triggerable one-shot)
 *               0 1 0 = Mode 2 (rate generator)
 *               0 1 1 = Mode 3 (square wave generator)
 *               1 0 0 = Mode 4 (software triggered strobe)
 *               1 0 1 = Mode 5 (hardware triggered strobe)
 *			 frequency - Frequency to Set for PIT
 *   OUTPUTS: Sets the interrupt rate 
 *   RETURN VALUE: return 0 on success, -1 on failure
 *   SIDE EFFECTS: None
 */   

int pit_init(int channel, int mode, int freq)
{	
	short command;	//init Command 
	if(freq>PIT_MAX_FREQ||freq<PIT_MIN_FREQ) //Check if it is below Max Freq
	{
		return -1;
	}
	

		command = (channel<<CHANNEL_BIT|LO_HIGH|(mode<<1)); //Concatenate to get the desired command
		int pit_freq_num = PIT_MAX_FREQ / freq;    //Change freq to desired input for PIT
		outb(command,PIT_CMD_PORT);             //Out b to right port with command and port
		outb(pit_freq_num & CLEAR_BIT,PIT_DATA_PORT);   
		outb(pit_freq_num >> PIT_HIGH_BYTE,PIT_DATA_PORT);     
		return 0;
	
}

/*
 *   get_next_task_number
 *   DESCRIPTION: It gets the next task number to switch to. Using the 
 *   current terminal number, we get the next task number to return. 
 *   INPUTS: None
 *   OUTPUTS: None 
 *   RETURN VALUE: Returns the next task number to switch to.
 *   SIDE EFFECTS: None
 */   

int32_t get_next_task_number()
{
	int32_t cur_task_terminal = get_current_terminal(); //Get current terminal number
	int32_t i;
	int32_t task_index;

	for(i = 1; i < NUM_TERMINALS; i++){ // For loop to find next terminal's top pcb to switch
		task_index = (i + cur_task_terminal) % NUM_TERMINALS; //Using the mod we calculate the task number to switch

		if(num_progs[task_index] > 0) //If we have found the task index and is running program greater than 0, we break.
			break;
	}

	return (i == NUM_TERMINALS) ? -1 : task_index; //Return the task index that we will be switching to. Otherwise return -1 for failure
}



