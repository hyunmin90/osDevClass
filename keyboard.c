#include "keyboard.h"
#include "lib.h"
#include "x86_desc.h"
#include "i8259.h"
#include "pcb.h"
#include "system_call.h"
#include "debug.h"

/* Keyboard R/W */
#define kbd_read_input()		inb(KBD_DATA_PORT)
#define kbd_read_status()		inb(KBD_STATUS_PORT)
#define kbd_write_output(val)	outb(val, KBD_DATA_PORT)
#define kbd_write_command(val)  outb(val, KBD_CNTL_PORT)

#define KEYS 0
#define SHIFT_KEYS 1
#define BACK 2

/* Keyboard Keys in Increasing Scan Code Order */
char keys[] = {
	'\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'-', '=', '\0', '\0', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
	'o', 'p', '[', ']', '\n', '\0', 'a', 's','d', 'f', 'g', 
	'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 
	'v', 'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' ', '\0'
};

/* Keyboard Keys when Shift is Pressed */
char shift_keys[] = {
	'\0', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
	'_', '+', '\0', '\0', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
	'O', 'P', '{', '}', '\n', '\0', 'A', 'S','D', 'F', 'G', 
	'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|','Z', 'X', 'C', 
	'V', 'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' ', '\0' 
};

char terminal_buf[NUM_TERMINALS][BUFFER_SIZE];	// Terminal Buffer
volatile int index[NUM_TERMINALS];				// Current Index of terminal_buf 

int alt_press = 0;
int shift_press = 0;
int ctrl_press = 0;
int caps_on = 0;

int read_on[NUM_TERMINALS] = {0, 0, 0};					//Indicates whether a read is being performed
volatile int read_return[NUM_TERMINALS] = {0, 0, 0};	// Indicates whether an enter has been pressed during a read


int32_t displayed_terminal = TERMINAL_1;	// Current Terminal on Display

extern int32_t num_progs[NUM_TERMINALS];
extern int screen_x[NUM_TERMINALS];
extern int screen_y[NUM_TERMINALS];
extern pcb_t* top_process[NUM_TERMINALS];

/* keyboard_handler()
   An Interrupt Handler that is Called When 
   Key is Pressed on Keyboard
   Input : i -- interrupt vector
   Output : None
   Side Effect : Handle keyboard interrupt
   				 Prints key Pressed to Screen
                 Issue EOI(End Of Interrupt) to unmask keyboard interrupt 
*/
void keyboard_handler(int i)
{
	/* Grab Key Pressed*/
	uint8_t keycode;
	keycode = kbd_read_input();
	int32_t curr_terminal= get_current_terminal();
	/* Print Only Keys Pressed, NOT Key Release 
	 * No Print for CTRL/SHIFT/Backspace/Caps
	 * Carries out Functionality Instead */
	if(keycode <= KEY_RELEASE_VALUE){
		if (keycode == ALT)
			alt_press += 1;

		else if(keycode == L_SHIFT || keycode == R_SHIFT)	
			shift_press += 1;

		else if (keycode == CTRL)
			ctrl_press += 1;

		else if (keycode == CAPS_LOCK)
			caps_on = !caps_on;

		else if(keycode == L && ctrl_press > 0)
			reset_screen();

		else if (keycode == BACKSPACE){
			if(read_on[curr_terminal] == 0)
				backspace();
			update_terminal_buf(BACK,keycode);
		}
		else if (read_on[curr_terminal] == 1 && keycode == RETURN){
			printf("\n");
			read_return[curr_terminal] = 1;
			update_terminal_buf(KEYS,keycode);
		}
		
		else if (caps_on == 1){
			if((read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE) || read_on[curr_terminal] == 0)
			caps_on_handler(keycode);
		}
		else if (shift_press > 0){
			if((read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE) || read_on[curr_terminal] == 0)
				printf("%c", shift_keys[keycode - 1]);
			update_terminal_buf(SHIFT_KEYS, keycode);
		}

		else if(alt_press > 0 && (keycode == F1 || keycode == F2 || keycode == F3)){
			switch(keycode){
				case F1:
					change_terminal(TERMINAL_1);
					break;
				case F2:
					change_terminal(TERMINAL_2);
					break;
				case F3:
					change_terminal(TERMINAL_3);
				default:
					break;
			}
		}
		else{
			if((read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE) || read_on[curr_terminal] == 0)
				printf("%c", keys[keycode - 1]);
			update_terminal_buf(KEYS, keycode);
		}

	}
	else if (keycode == (ALT + KEY_RELEASE_VALUE))
		alt_press -= 1;

	else if(keycode == (L_SHIFT + KEY_RELEASE_VALUE) || keycode == (R_SHIFT + KEY_RELEASE_VALUE))
		shift_press -= 1;

	else if (keycode == (CTRL + KEY_RELEASE_VALUE))
		ctrl_press -= 1;


	send_eoi(KEYBOARD_IRQ);
}

/* letter_check function
   Function Checks wether or not input keycode corresponds
   To the Scancode of a alphabet char
   Input : keycode -- scancode of key pressed
   Output : Returns 1 if keycode is letter
   			0 otherwise
   Side Effect : Checks for a letter
*/
int letter_check(unsigned char keycode)
{
	if(Q <= keycode && keycode <= P)
		return 1;
	else if(A <= keycode && keycode <= L)
		return 1;
	else if (Z <= keycode && keycode <= M)
		return 1;
	else
		return 0;
}

/* caps_on_handler function
   Function handles the caps on case for keyboard handler
   Prints out according to keycode
   Input : keycode -- scancode of key pressed
   Output : None
   Side Effect : Calls letter_check function
   				 Updates the Char terminal_buffer
   				 Prints to Screen corresponding string
*/
void caps_on_handler(unsigned char keycode)
{
	int32_t curr_terminal = get_current_terminal();
	/* Shift is pressed with Caps On */
	if(shift_press > 0){
		 /* Use lowercase keys for letters */
		if(letter_check(keycode)){
			printf("%c", keys[keycode - 1]);
			if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
				terminal_buf[curr_terminal][index[curr_terminal]] = keys[keycode - 1];
				index[curr_terminal]++;
			}
		}
		/* Use shift version of keys for all others*/
		else{
			printf("%c", shift_keys[keycode - 1]);
			if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
				terminal_buf[curr_terminal][index[curr_terminal]] = shift_keys[keycode - 1];
				index[curr_terminal]++;
			}
		}
	}
	/* Shift is not pressed */
	else{
		/* Use uppercase keys for letters */
		if(letter_check(keycode)){
			printf("%c", shift_keys[keycode - 1]);
			if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
				terminal_buf[curr_terminal][index[curr_terminal]] = shift_keys[keycode - 1];
				index[curr_terminal]++;
			}
		}
		/* use normal version of keys for all others */
		else{
			printf("%c", keys[keycode - 1]);
			if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
				terminal_buf[curr_terminal][index[curr_terminal]] = keys[keycode - 1];
				index[curr_terminal]++;
			}
		}
	}
}

/* change_terminal function
   Function handles switching terminals (ALT + FUNCTION-KEY)
   Input : new_terminal -- terminal to switch into
   Output : None
   Side Effect : Does nothing if the new terminal 
   is the same as the current terminal.
   Else it copies video memory into the correct video buffer
   and the correct video buffer into video memory
*/
void change_terminal(int32_t new_terminal)
{
	/* Check to make sure that the new_terminal is not the same
	   as the terminal displayed now */
	int32_t curr_terminal = get_displayed_terminal();
	if(new_terminal == curr_terminal){
		LOG("You are already in this terminal! Cannot Switch!\n");
		return;
	}

	if(num_progs[TERMINAL_1] + num_progs[TERMINAL_2] + num_progs[TERMINAL_3] >= MAX_NUM_PROCESS && 
	   num_progs[new_terminal] == 0){
	   	printf("Reached maximum number of programs! Can't fire new shell in new terminal!391OS> ");
		return;
	}

	/* Grab the current page dir for remapping */
	pcb_t* pcb_ptr = get_pcb_ptr();
	pde_t* current_pg_dir = pcb_ptr->pg_dir;

	/* Copy Video Memory into the Buffer corresponding to the current terminal
	   Remap the current page dir to map the physical buffer as the video */
	switch(curr_terminal){
		case TERMINAL_1:
			remap_page(USER_VIDEO, VIDEO_BUF_1, 
			PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, current_pg_dir);
			memcpy((char *)VIDEO_BUF_1, (char *)VIDEO, (NUM_COLS * (NUM_ROWS) * 2));
			break;
		case TERMINAL_2:
			remap_page(USER_VIDEO, VIDEO_BUF_2, 
			PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, current_pg_dir);
			memcpy((char *)VIDEO_BUF_2, (char *)VIDEO, (NUM_COLS * (NUM_ROWS) * 2));
			break;
		case TERMINAL_3:
			remap_page(USER_VIDEO, VIDEO_BUF_3, 
			PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, current_pg_dir);
			memcpy((char *)VIDEO_BUF_3, (char *)VIDEO, (NUM_COLS * (NUM_ROWS) * 2));
			break;
		default:
			LOG("Should not happen! Current Terminal does not exist??\n");
			return;
	}

	pcb_t* top_pcb = top_process[new_terminal];

	/* Copy the right video buf into video mem */
	switch(new_terminal){
		case TERMINAL_1:
			if(top_pcb != NULL)
				remap_page(USER_VIDEO, VIDEO, 
				PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, top_pcb -> pg_dir);
			memcpy((char *)VIDEO, (char *)VIDEO_BUF_1, (NUM_COLS * (NUM_ROWS) * 2));		
			break;
		case TERMINAL_2:
			if(top_pcb != NULL)
				remap_page(USER_VIDEO, VIDEO, 
				PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, top_pcb -> pg_dir);
			memcpy((char *)VIDEO, (char *)VIDEO_BUF_2, (NUM_COLS * (NUM_ROWS) * 2));
			break;
		case TERMINAL_3:
			if(top_pcb != NULL)
				remap_page(USER_VIDEO, VIDEO, 
				PAGING_USER_SUPERVISOR | PAGING_READ_WRITE, top_pcb -> pg_dir);
			memcpy((char *)VIDEO, (char *)VIDEO_BUF_3, (NUM_COLS * (NUM_ROWS) * 2));
			break;
		default:
			LOG("Cannot switch into terminal. Chosen terminal does not exist!\n");
			return;
	}

	/* Update the displayed terminal*/
	displayed_terminal = new_terminal;

	/* If the the new terminal is not executing any programs, Execute Shell */
	if(num_progs[new_terminal] == 0){
		send_eoi(KEYBOARD_IRQ);		//Send EOI for ALT+FUNCTION-KEY
	
		asm volatile("sti;");	

		uint8_t exec_cmd[15] = "shell";
		pcb_ptr -> esp0 = tss.esp0; 
		asm volatile("movl %%esp, %0;" 
	 		"movl %%ebp, %1;" : 
	 		"=b" (pcb_ptr -> esp),
	 		"=c" (pcb_ptr -> ebp));

		/* Execute a Shell */
		if(-1 == sys_execute(exec_cmd)){
			LOG("Executing new shell from new terminal failed!\n");
		}
	}
	else{
		/* Else update the screen with the previous x,y
		 * Flush the TLB's */
	 	send_eoi(KEYBOARD_IRQ);
	 	set_cr3_reg(top_pcb -> pg_dir);
	 	update_cursor(screen_y[new_terminal],screen_x[new_terminal]);
	 	pcb_ptr -> esp0 = tss.esp0;
	 	asm volatile("movl %%esp, %0;" 
	 		"movl %%ebp, %1;" : 
	 		"=b" (pcb_ptr -> esp),
	 		"=c" (pcb_ptr -> ebp));

	 	tss.esp0 = top_pcb -> esp0;
		asm volatile("movl %0, %%esp;" 
			"movl %1, %%ebp;":: 
			"b" (top_pcb -> esp),
			"c" (top_pcb -> ebp));
	}
}

/* Get_Current_Terminal function
   Returns the terminal that the caller is executing in
   Input : None
   Output : Terminal Number that the caller is executing in
   Side Effect : None
*/
int32_t get_current_terminal()
{
	pcb_t* pcb_ptr = get_pcb_ptr();
	return (pcb_ptr -> terminal_num);
}

/* Get_Displayed_Terminal function
   Returns the terminal that is currently being displayed
   Input : None
   Output : Terminal Number of the terminal currently displayed
   Side Effect : None
*/
int32_t get_displayed_terminal()
{
	return displayed_terminal;
}

/* Terminal Open function
   Input : None
   Output : return 0 on success
   Side Effect : Keyboard Interrupts are enabled
*/
int32_t terminal_open()
{
	enable_irq(KEYBOARD_IRQ);
	return 0;
}

/* Terminal Close function
   Input : None
   Output : return 0 on success
   Side Effect : Disables Keyboard Interrupts
   				 Moves Cursor to Top Left
   				 Clears Screen 
*/
int32_t terminal_close()
{
	disable_irq(KEYBOARD_IRQ);
	update_cursor(0,0);
	clear();
	return 0;
}

/* Terminal Read function
   Input : dummy -- Dummy Variable to Align Args
   		   buf -- char buf to copy data to
   		   nbytes -- number of bytes to copy into buf
   Output : # of bytes copied into char buf
   Side Effect : Fills in buffer with nbytes or line terminated with enter
*/
int32_t terminal_read(uint32_t dummy, uint8_t* buf,uint32_t nbytes)
{
	int32_t curr_terminal = get_current_terminal();
	index[curr_terminal] = 0;		// Initalize index
	read_on[curr_terminal] = 1;	// Currently reading
	int i = 0;
	for(i=0;i < nbytes;i++)
	{
		buf[i]='\0';
	}
	
	/* Wait Until reached the num bytes requested
	 * Or line terminated with enter
	 * Or Keyboard Buffer Size */
	if(nbytes <= MAX_ARG_BUF){
		while(read_return[curr_terminal] == 0 && index[curr_terminal] < nbytes && index[curr_terminal] < BUFFER_SIZE){}

		/* Store read bytes into buf */
		int i;
		for(i = 0; i < nbytes; i++){
			if(i < index[curr_terminal])
				buf[i] = terminal_buf[curr_terminal][i];
			else
				buf[i] = NULL;
		}
		/* Reset Variables(Not Reading) */
		read_on[curr_terminal] = 0;
		read_return[curr_terminal] = 0;
		if(index[curr_terminal] == nbytes)
			printf("\n");
		return index[curr_terminal];
	}
	/* Reset Variables*/
	read_on[curr_terminal] = 0;
	read_return[curr_terminal] = 0;
	return -1;
}

/* Terminal Write function
   Input : dummy -- Dummy Variable to Align Args
   		   dummy1 -- Dummy Variable to Align Args
   		   buf - char buf with data to write to terminal
   		   nbytes -- number of bytes to write to terminal
   Output : Return number of bytes written to terminal including '\n' if any
   Side Effect : Writes to Terminal 
*/
int32_t terminal_write(uint32_t dummy, uint32_t dummy1, const uint8_t* buf, uint32_t nbytes)
{	
	int i;
	int n = 0;
	if(buf == NULL)
		return -1;
	if(nbytes <= 0)
		return -1;
	/* Print string in buf until 
	 * Reached End of String */
	for (i = 0; i < nbytes; i++) {
		if (buf[i] != NULL) {
			putc(buf[i]);
			n++;
		} else {
		}
	}
	/* Return # of bytes written */
	return i + 1;
	//return (n == 0)? -1 : n;
}

/* update_terminal_buf function
   Input : n -- Value that tells how the function should update the buffer
   		   Tells what this func responds to
   		   keycode -- keycode of button recently pressed
   Output : None
   Side Effect : Updates the terminal_buf accordingly, and changes index
*/
void update_terminal_buf(int n, char keycode)
{
	int32_t curr_terminal = get_current_terminal();
	if(n == KEYS){
		if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
			terminal_buf[curr_terminal][index[curr_terminal]] = keys[keycode - 1];
			index[curr_terminal]++;
		}
	}
	else if (n == SHIFT_KEYS){
		if(read_on[curr_terminal] == 1 && index[curr_terminal] < BUFFER_SIZE){
			terminal_buf[curr_terminal][index[curr_terminal]] = shift_keys[keycode - 1];
			index[curr_terminal]++;
		}
	}
	else if(n == BACK){
		if(index[curr_terminal] != 0 && read_on[curr_terminal] == 1){
			backspace();
			terminal_buf[curr_terminal][index[curr_terminal] - 1] = NULL;
			index[curr_terminal]--;
		}
	}
}

/* test_terminal function
   Input : None
   Output : None
   Side Effect : Tests the terminal with open/write/read
*/
void test_terminal(void)
{
	int buf_size;
	buf_size = 20;
	int f;
	int g;
	int p = terminal_open();
	if(p == 0){
		unsigned char buf[buf_size];
		while(1)
		{
			f = terminal_read(0,buf, buf_size);
			if(f != -1){
				g = terminal_write(0,0,buf, buf_size);
			}
			int q;
			for (q = 0; q < buf_size; q++) {
				buf[q] = 0;
			}
		}
	}
}
