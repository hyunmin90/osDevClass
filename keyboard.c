#include "keyboard.h"
#include "lib.h"
#include "x86_desc.h"
#include "i8259.h"

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

int shift_press = 0;
int ctrl_press = 0;
int caps_on = 0;
int read_on = 0;
volatile int read_return = 0;
volatile int index;

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
	
	/* Print Only Keys Pressed, NOT Key Release 
	 * No Print for CTRL/SHIFT/Backspace/Caps
	 * Carries out Functionality Instead */
	if(keycode <= KEY_RELEASE_VALUE){
		if(keycode == L_SHIFT || keycode == R_SHIFT)	
			shift_press += 1;

		else if (keycode == CTRL)
			ctrl_press += 1;

		else if (keycode == CAPS_LOCK)
			caps_on = !caps_on;

		else if(keycode == L && ctrl_press > 0)
			reset_screen();

		else if (keycode == BACKSPACE){
			if(read_on == 0)
				backspace();
			update_terminal_buf(BACK,keycode);
		}
		else if (read_on == 1 && keycode == RETURN){
			printf("\n");
			read_return = 1;
			update_terminal_buf(KEYS,keycode);
		}
		
		else if (caps_on == 1){
			if((read_on == 1 && index < BUFFER_SIZE) || read_on == 0)
			caps_on_handler(keycode);
		}
		else if (shift_press > 0){
			if((read_on == 1 && index < BUFFER_SIZE) || read_on == 0)
				printf("%c", shift_keys[keycode - 1]);
			update_terminal_buf(SHIFT_KEYS, keycode);
		}

		else{
			if((read_on == 1 && index < BUFFER_SIZE) || read_on == 0)
				printf("%c", keys[keycode - 1]);
			update_terminal_buf(KEYS, keycode);
		}
	}
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
	/* Shift is pressed with Caps On */
	if(shift_press > 0){
		 /* Use lowercase keys for letters */
		if(letter_check(keycode)){
			printf("%c", keys[keycode - 1]);
			if(read_on == 1 && index < BUFFER_SIZE){
				terminal_buf[index] = keys[keycode - 1];
				index++;
			}
		}
		/* Use shift version of keys for all others*/
		else{
			printf("%c", shift_keys[keycode - 1]);
			if(read_on == 1 && index < BUFFER_SIZE){
				terminal_buf[index] = shift_keys[keycode - 1];
				index++;
			}
		}
	}
	/* Shift is not pressed */
	else{
		/* Use uppercase keys for letters */
		if(letter_check(keycode)){
			printf("%c", shift_keys[keycode - 1]);
			if(read_on == 1 && index < BUFFER_SIZE){
				terminal_buf[index] = shift_keys[keycode - 1];
				index++;
			}
		}
		/* use normal version of keys for all others */
		else{
			printf("%c", keys[keycode - 1]);
			if(read_on == 1 && index < BUFFER_SIZE){
				terminal_buf[index] = keys[keycode - 1];
				index++;
			}
		}
	}
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
	index = 0;		// Initalize index
	read_on = 1;	// Currently reading
	int i = 0;
	for(i=0;i<nbytes;i++)
	{
		buf[i]='\0';
	}
	
	/* Wait Until reached the num bytes requested
	 * Or line terminated with enter
	 * Or Keyboard Buffer Size */
	if(nbytes <= MAX_ARG_BUF){
		while(read_return == 0 && index < nbytes && index < BUFFER_SIZE){}

		/* Store read bytes into buf */
		int i;
		for(i = 0; i < nbytes; i++){
			if(i < index)
				buf[i] = terminal_buf[i];
			else
				buf[i] = NULL;
		}
		/* Reset Variables(Not Reading) */
		read_on = 0;
		read_return = 0;
		if(index == nbytes)
			printf("\n");
		return index;
	}
	/* Reset Variables*/
	read_on = 0;
	read_return = 0;
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
	if(nbytes < 0)
		return -1;
	/* Print string in buf until 
	 * Reached End of String */
	for (i = 0; i < nbytes; i++) {
		if (buf[i] != NULL) {
			putc(buf[i]);
			n++;
		} else {
			break;
		}
	}
	if(nbytes == 0)
		return -1;
	/* Return # of bytes written */
	return i+1;
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
	if(n == KEYS){
		if(read_on == 1 && index < BUFFER_SIZE){
			terminal_buf[index] = keys[keycode - 1];
			index++;
		}
	}
	else if (n == SHIFT_KEYS){
		if(read_on == 1 && index < BUFFER_SIZE){
			terminal_buf[index] = shift_keys[keycode - 1];
			index++;
		}
	}
	else if(n == BACK){
		if(index != 0 && read_on == 1){
			backspace();
			terminal_buf[index - 1] = NULL;
			index--;
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
