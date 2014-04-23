/* rtc.h - Defines used in interactions with the rtc controller
 */


#ifndef _RTC_H
#define _RTC_H

#include "types.h"

/* Ports for RTC */
#define	RTC_CMD_PORT 0x70
#define	RTC_DATA_PORT 0x71

/* Registers for RTC */
#define	RTC_REGA 0x0A
#define	RTC_REGB 0x0B
#define	RTC_REGC 0x0C

/*IRQ value representing RTC */
#define RTC_IRQ 8

/* Periodic interrupt enable value:
this enables the periodic interrupt.
Represents the bit 6 of Register B */
#define RTC_PIE 0x40

/* sample frequency rate */
#define FREQ1 0x01	// 256Hz
#define FREQ2 0x02  // 128HZ
#define FREQ3 0x03	// 8.192kHz
#define FREQF 0x0F	// 2Hz

#define MAXFREQ 1024 //Max frequency 1024
#define WRONGFREQ 20 //Wrong frequency in range.not in power of two
#define CBIT1 0x0F //Clear the bits not related.
#define CBIT2 0xF0 //Clear bits not related

#define NUM_FREQ	11

/* Handle RTC */
void rtc_handler(int i);
/* Initialize RTC */
void rtc_init();
/*Open rtc. Set the freq to 2hz*/
int32_t rtc_open(int32_t dummy);
/*Read from RTC. Wait until rtc interrupt occurs*/
int32_t rtc_read();
/*Write the frequency desired */
int32_t rtc_write(uint32_t dummy, uint32_t dummy1, int32_t* freq_ptr, uint32_t dummy2);
/*Close rtc*/
int32_t rtc_close();
/*Test case for rtc open read write close*/
void rtc_test();


/* Test_interrupt, defined in lib.c:
this line is to simply avoid warning */
extern void test_interrupts();

/*Variable that checks if RTC interrupt occured or not*/
volatile int RTCreadCheck;


#endif /* _RTC_H */



