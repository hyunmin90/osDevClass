/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"
 
#define MASK_SLAVE		0xFF	// mask all interrupt lines
#define MASK_MASTER   	0xFB	// mask all except IRQ2(connection to slave)

/* Interrupt masks to determine which interrupts
 * are enabled and disabled */
uint8_t master_mask; /* IRQs 0-7 */
uint8_t slave_mask; /* IRQs 8-15 */

/* i8259_init()
   Initialize the cascaded layout of two 8259 PICs
   Mask all interrupts except for master PIC's IRQ2 which is connected to slave PIC
   Input : None
   Output : None
   Side Effects : Initializes I8259 PICs, mask all interrupts
*/
void
i8259_init(void) {
    /* start of the initialization */
    outb(ICW1, MASTER_8259_PORT);
    outb(ICW1, SLAVE_8259_PORT);

    /*initialization command words 2 : vector offsets
      Master PIC starts at 0x20, and takes up 8 numbers
      Slave PIC starts at 0x28 and takes up 8 numbers
    */
    outb(ICW2_MASTER, MASTER_DATA);
    outb(ICW2_SLAVE, SLAVE_DATA);

    /* Initialization command words 3 :
       Tell master PIC that a slave PIC exists at IRQ 2 (In binary, 0000 0100)
       Tell slave its CAS position (2 as in IRQ2)
    */
    outb(ICW3_MASTER, MASTER_DATA);
    outb(ICW3_SLAVE, SLAVE_DATA);


    /* initialization command words 4 
       Tell the processor is 80x86
    */
    outb(ICW4, MASTER_DATA);
    outb(ICW4, SLAVE_DATA);

    // Mask all interrupts(Except IRQ2)
    outb(MASK_MASTER, MASTER_DATA);
    outb(MASK_SLAVE, SLAVE_DATA);

}

/* enable_irq()
   Enable (unmask) the specified IRQ 
   Input : irq_num -- IRQ number to be enabled takes from 0 to 15
   Output : None
   Side Effect : Enables interrupt on given IRQ number
*/
void
enable_irq(uint32_t irq_num)
{
    if (irq_num >= IRQs) {
        // slave case : irq_num -8 will make the number format same as master
        irq_num -= IRQs;
        /* Get slave PIC's current mask and unmask only the specified IRQ leaving 
           others as it is */
        slave_mask = inb(SLAVE_DATA) & ~(1 << irq_num);
        outb(slave_mask, SLAVE_DATA);
    } else {
        /* Get master PIC's current mask and unmask only the specified IRQ leaving 
           others as it is */
        master_mask = inb(MASTER_DATA) & ~(1 << irq_num);
        outb(master_mask, MASTER_DATA);	
    }
}

/* disable_irq()
   Disable (mask) the specified IRQ 
   Input : irq_num -- IRQ number to be disabled; takes from 0 to 15
   Output : None
   Side Effect : Disables interrupt on given IRQ number
*/
void
disable_irq(uint32_t irq_num)
{
    if(irq_num >= IRQs) {
        // slave case : irq_num -8 will make the number format same as master
        irq_num -= IRQs;
        /* Get slave PIC's current mask and mask only the specified IRQ leaving 
           others as it is */
        slave_mask = inb(SLAVE_DATA) | (1 << irq_num);
        outb(slave_mask, SLAVE_DATA);
    } else {
        /* Get master PIC's current mask and mask only the specified IRQ leaving 
           others as it is */
        master_mask = inb(MASTER_DATA) | (1 << irq_num);
        outb(master_mask, MASTER_DATA);	
    }
}

/* send_eoi()
   Send end-of-interrupt signal for the specified IRQ
   Input : irq_num -- IRQ number to be mark as ended; takes from 0 to 15
   Output : None
   Side Effects : Change internal states of the PIC, so that PIC can raise
                  new interrupts on the same or lower privileged IRQs
 */
void
send_eoi(uint32_t irq_num)
{
    // if IRQ is greater than 7, both master and slave PIC are called 
    if(irq_num >= IRQs)
    {
        irq_num -= IRQs;
        outb(EOI | irq_num, SLAVE_8259_PORT);
        outb(EOI | SLAVEIRQ, MASTER_8259_PORT);
    }
    // if not, just call master PIC
    else outb(EOI | irq_num, MASTER_8259_PORT);
}

