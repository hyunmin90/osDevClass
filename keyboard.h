#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"
/* Keyboard Ports */
#define KBD_DATA_PORT 		0x60
#define KBD_CNTL_PORT		0x64
#define KBD_STATUS_PORT		0x64	


#define KEY_RELEASE_VALUE 	0x80	//	Release Key Scan Code Offset

/* Scan Code Values */
#define L_SHIFT				0x2A
#define R_SHIFT 			0x36
#define CAPS_LOCK 			0x3A
#define BACKSPACE 			0x0E
#define CTRL				0x1D
#define RETURN 				0x1C

#define Q					0x10
#define P 					0x19
#define A					0x1E
#define L 					0x26
#define Z					0x2C
#define M					0x32

#define ALT					0x38
#define F1					0x3B
#define F2					0x3C
#define F3					0x3D

#define BUFFER_SIZE 		128
#define MAX_ARG_BUF			1024

/*IRQ value representing keyboard */
#define KEYBOARD_IRQ 	1

#define NUM_TERMINALS 	3
#define TERMINAL_1		0
#define TERMINAL_2		1
#define TERMINAL_3 		2

/* Handler for Keyboard Interrupts(Key Presses) */
void keyboard_handler(int i);
/* Helper Function that checks if a letter has been pressed */
int letter_check(unsigned char keycode);
/* Helper Function for Keyboard handler for Caps_On Case */
void caps_on_handler(unsigned char keycode);

/* Opens Terminal */
int32_t terminal_open();
/* Close Terminal */
int32_t terminal_close();
/* Read from Terminal */
int32_t terminal_read(uint32_t dummy, uint8_t* buf,uint32_t nbytes);
/* Write to Terminal */
int32_t terminal_write(uint32_t dummy, uint32_t dummy1, const uint8_t* buf, uint32_t nbytes);
/* Helper function to update terminal buf */
void update_terminal_buf(int n, char keycode);
/* Tests terminal open/close/read */
void test_terminal(void);

void change_terminal(int32_t new_terminal);

int32_t get_current_terminal();
int32_t get_displayed_terminal();

#endif 
