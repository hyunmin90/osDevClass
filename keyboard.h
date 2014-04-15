#ifndef _KEYBOARD_H
#define _KEYBOARD_H

/* Keyboard Ports */
#define KBD_DATA_PORT 			0x60
#define KBD_CNTL_PORT			0x64
#define KBD_STATUS_PORT			0x64	


#define KEY_RELEASE_VALUE 		0x80	//	Release Key Scan Code Offset

/* Scan Code Values */
#define L_SHIFT					0x2A
#define R_SHIFT 				0x36
#define CAPS_LOCK 				0x3A
#define BACKSPACE 				0x0E
#define CTRL					0x1D
#define RETURN 					0x1C

#define Q						0x10
#define P 						0x19
#define A						0x1E
#define L 						0x26
#define Z						0x2C
#define M						0x32

#define BUFFER_SIZE 			128

/*IRQ value representing keyboard */
#define KEYBOARD_IRQ 	1

/* Handler for Keyboard Interrupts(Key Presses) */
void keyboard_handler(int i);
/* Helper Function that checks if a letter has been pressed */
int letter_check(unsigned char keycode);
/* Helper Function for Keyboard handler for Caps_On Case */
void caps_on_handler(unsigned char keycode);

/* Opens Terminal */
int terminal_open(void);
/* Close Terminal */
int terminal_close(void);
/* Read from Terminal */
int terminal_read(char *buf,int nbytes);
/* Write to Terminal */
int terminal_write(const char *buf, int nbytes);
/* Helper function to update terminal buf */
void update_terminal_buf(int n, char keycode);
/* Tests terminal open/close/read */
void test_terminal(void);

/* Terminal Buffer */
char terminal_buf[BUFFER_SIZE];
/* Current Index of terminal_buf */
volatile int index;
/* Tells if enter has been pressed during terminal read */
volatile int read_return;

#endif 
