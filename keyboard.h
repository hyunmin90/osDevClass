#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"
#include "paging.h"

/* Keyboard Ports */
#define KBD_DATA_PORT 		0x60
#define KBD_CNTL_PORT		0x64
#define KBD_STATUS_PORT		0x64	

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

/* Release Key Scan Code Offset */
#define KEY_RELEASE_VALUE 	0x80	

/* Terminal Buffer Size */
#define BUFFER_SIZE 		128
#define MAX_ARG_BUF			1024

/* Terminal Information */
#define NUM_TERMINALS 	3
#define TERMINAL_1		0
#define TERMINAL_2		1
#define TERMINAL_3 		2

/* Type of Button Press */
#define KEYS 0
#define SHIFT_KEYS 1
#define BACK 2

/*IRQ value representing keyboard */
#define KEYBOARD_IRQ 	1

/* Keyboard R/W */
#define kbd_read_input()		inb(KBD_DATA_PORT)
#define kbd_read_status()		inb(KBD_STATUS_PORT)
#define kbd_write_output(val)	outb(val, KBD_DATA_PORT)
#define kbd_write_command(val)  outb(val, KBD_CNTL_PORT)

/* Handler for Keyboard Interrupts(Key Presses) */
void keyboard_handler(int i);
/* Helper Function that checks if a letter has been pressed */
int letter_check(unsigned char keycode);
/* Helper Function for Keyboard handler for Caps_On Case */
void caps_on_handler(unsigned char keycode);
/* Helper function to update terminal buf */
void update_terminal_buf(int n, char keycode);
/* Returns if key has been pressed */
int get_key_press(void);

/* Opens Terminal */
int32_t terminal_open();
/* Close Terminal */
int32_t terminal_close();
/* Read from Terminal */
int32_t terminal_read(uint32_t dummy, uint8_t* buf,uint32_t nbytes);
/* Write to Terminal */
int32_t terminal_write(uint32_t dummy, uint32_t dummy1, const uint8_t* buf, uint32_t nbytes);
/* Tests terminal open/close/read */
void test_terminal(void);

/* Switches Terminal for ALT-FUNCTION Press */ 
void change_terminal(int32_t new_terminal);
/* Helper for change_terminal */
void remap_user_video_and_memcpy(int32_t remap_to_addr, int32_t remap_from_addr, pde_t* pg_dir);
/* Helper for change_terminal */
int32_t get_video_buf_for_terminal(int32_t terminal_num);

/* Gets terminal of current process */
int32_t get_current_terminal();
/* Gets displayed terminal */
int32_t get_displayed_terminal();

#endif 
