#include "interrupt_handler.h"
#include "x86_desc.h"
#include "lib.h"
#include "debug.h"
#include "keyboard.h"
#include "rtc.h"

/* Build assembly linkages for exceptions */
BUILD_IRQ(0x00)
BUILD_IRQ(0x01)
BUILD_IRQ(0x02)
BUILD_IRQ(0x03)
BUILD_IRQ(0x04)
BUILD_IRQ(0x05)
BUILD_IRQ(0x06)
BUILD_IRQ(0x07)
BUILD_IRQ_ERRCODE(0x08)
BUILD_IRQ(0x09)
BUILD_IRQ_ERRCODE(0x0a)
BUILD_IRQ_ERRCODE(0x0b)
BUILD_IRQ_ERRCODE(0x0c)
BUILD_IRQ_ERRCODE(0x0d)
BUILD_IRQ_ERRCODE(0x0e)
BUILD_IRQ(0x0f)
BUILD_IRQ(0x10)
BUILD_IRQ_ERRCODE(0x11)
BUILD_IRQ(0x12)
BUILD_IRQ(0x13)
BUILD_IRQ(0x14)

/* Build assembly linkages for other interrupts */
BUILD_IRQ(0x20)
BUILD_IRQ(0x21)
BUILD_IRQ(0x22)
BUILD_IRQ(0x23)
BUILD_IRQ(0x24)
BUILD_IRQ(0x25)
BUILD_IRQ(0x26)
BUILD_IRQ(0x27)
BUILD_IRQ(0x28)
BUILD_IRQ(0x29)
BUILD_IRQ(0x2a)
BUILD_IRQ(0x2b)
BUILD_IRQ(0x2c)
BUILD_IRQ(0x2d)
BUILD_IRQ(0x2e)
BUILD_IRQ(0x2f)

/* Build assembly linkage for system Call */
BUILD_SYSCALL(0x80)

/* Assembly linkage functions for exceptions */
static void (*exception[NR_EXCEPTIONS])(void) = {
    IRQ0x00_interrupt,
    IRQ0x01_interrupt, IRQ0x02_interrupt, IRQ0x03_interrupt,
    IRQ0x04_interrupt, IRQ0x05_interrupt, IRQ0x06_interrupt,
    IRQ0x07_interrupt, IRQ0x08_interrupt, IRQ0x09_interrupt,
    IRQ0x0a_interrupt, IRQ0x0b_interrupt, IRQ0x0c_interrupt,
    IRQ0x0d_interrupt, IRQ0x0e_interrupt, IRQ0x0f_interrupt,
    IRQ0x10_interrupt, IRQ0x11_interrupt, IRQ0x12_interrupt,
    IRQ0x13_interrupt, IRQ0x14_interrupt
};

/* Assembly linkage functions for interrupts */
static void (*interrupt[NR_IRQS])(void) = {
    IRQ0x20_interrupt, IRQ0x21_interrupt, IRQ0x22_interrupt,
    IRQ0x23_interrupt, IRQ0x24_interrupt, IRQ0x25_interrupt,
    IRQ0x26_interrupt, IRQ0x27_interrupt, IRQ0x28_interrupt,
    IRQ0x29_interrupt, IRQ0x2a_interrupt, IRQ0x2b_interrupt,
    IRQ0x2c_interrupt, IRQ0x2d_interrupt, IRQ0x2e_interrupt,
    IRQ0x2f_interrupt
};
/* common_handler()
   A common interrupt handler that gets called every time an exception,
   interrupt, or system call is invoked.
   Input : i -- interrupt vector
   Output : None
   Side Effect : Handle interrupt(or exception and system call)
   Issue EOI(End Of Interrupt) to unmask handled interrupt   
   Saves and Restores Regs
*/
void common_handler(int i) {
    //SAVE_ALL
    /* Exceptions */
    if(i >= VEC_LOWEST_EXCEPTION && i <= VEC_HIGHEST_EXCEPTION) {
        /* Call Halt to Squash Exceptions */
        printf("Exception %x Reached\n", i);
        /*int p;
        asm volatile("movl %%cr2, %0;" ::"b"(p));
        printf("%x", p);*/
        asm volatile("movl $1, %%eax; movl %0, %%ebx;int $0x80;"::"b"(256));
    } 
    /* Regular Interrupts */
    else if (i >= VEC_LOWEST_IRQ && i <= VEC_HIGHEST_IRQ) {
        if (i == VEC_KEYBOARD_INT) {
            /* In case of keyboard interrupt, hand control over to keyboard_handler */
            keyboard_handler(i);
        } else if(i == VEC_RTC_INT) {
            /* In case of RTC interrupt, hand control over to rtc_handler */
            RTCreadCheck = 1;//The RTCReadcheck has been enabled
            rtc_handler(i);	
        } else {
            printf("Interrupts %x Reached\n", i);
        }
    } else {
        printf("Undefined Interrupt / Exception Reached\n");
    }
    //RESTORE_ALL
}

/* init_idt()
   Initialize IDT and its entries
   1. Initialize x86 defined exception vectors from 0x00 to 0x1F
   2. Initialize device interrupt vectors from 0x20 to 0x2F
   3. Initialize IDT entry for system call which has vector 0x80
   Input : None
   Output : None
   Side Effects : Generate IDT and fill its entries
                  Change IDTR to be pointing to the beginning of IDT
*/
void init_idt(void) {
    int i;
    for(i = 0; i < 256; i++) {
        /* Set up idt entry */
        idt_desc_t the_idt_desc;
        the_idt_desc.seg_selector   = KERNEL_CS;
        the_idt_desc.reserved4		= 0;
        the_idt_desc.reserved3 		= 0;
        the_idt_desc.reserved2		= 1;
        the_idt_desc.reserved1		= 1;
        the_idt_desc.size		    = 1;
        the_idt_desc.reserved0		= 0;
        the_idt_desc.dpl		    = 0;
        the_idt_desc.present		= 1;

        /* Branch by different interrupt vectors */
        if (i >= VEC_LOWEST_EXCEPTION && i <= VEC_HIGHEST_EXCEPTION) {
            /* Exception vectors defined by Intel */
            SET_IDT_ENTRY(the_idt_desc, exception[i]);
        } else if (i >= VEC_LOWEST_IRQ && i <= VEC_HIGHEST_IRQ) {
            /* Device interrupts */
            SET_IDT_ENTRY(the_idt_desc, interrupt[i - NUM_INTEL_DEFINED]);
        } else if (i == VEC_SYSTEM_CALL) {
            /* For system call, set dpl to 3 to enable normal users with
               user privilege to call it */
            the_idt_desc.reserved3 = 1;         // Set idt using Trap Gate(111)
            the_idt_desc.dpl = USER_LEVEL;
            SET_IDT_ENTRY(the_idt_desc, IRQ0x80_interrupt);
        } else {
            /* Do not register IDT entry for undefined vector */
            continue;
        }
                
        /* Register IDT entry to the IDT */
        idt[i] = the_idt_desc;
    }

    /* Load the IDT's beginning physical address into IDTR */
    lidt(idt_desc_ptr);
}
