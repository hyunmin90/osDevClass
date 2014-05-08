#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "types.h"


/*IRQ value representing PIT */
#define PIT_IRQ 0


/* Ports for RTC */
#define PIT_CMD_PORT 0x43
#define PIT_DATA_PORT 0x40
/*IRQ0 for PIT*/
#define PIT_IRQ 0
/*Pre defined PIT command mode*/
#define PIT_SCHEDULER 0x36 

/*Define frequency*/
#define PIT_MAX_FREQ 1193182
#define PIT_MIN_FREQ 19
/*Mode of PIT*/
#define LO_HIGH 0x30
/*Number of Terminal*/
#define  NUM_TERMINALS 	3
/*AND CLEAR BIT*/
#define CLEAR_BIT 0xFF

#define CHANNEL_BIT 6
#define PIT_HIGH_BYTE	8
int pit_init(int channel, int mode, int freq);

void pit_handler();
void switch_task(int32_t new_task_number);
int32_t get_next_task_number();

#endif 
