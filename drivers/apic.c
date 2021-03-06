/// Copyright (C) strawberryhacker 

#include "citrus-boot/apic.h"
#include <citrus-boot/matrix.h>
#include <citrus-boot/print.h>
#include <citrus-boot/bitops.h>

#define PERIPH_CNT 72

/// Returns the correct APIC controller (secure / non-secure) based on the IRQ 
/// number. This will also check the state of the SFR redirection bits
static struct apic_reg* get_apic(u8 irq)
{
    struct matrix_reg* matrix = get_matrix(irq);
    if (is_secure(matrix, irq)) {
        // If secure interrupts are redirected - force the APIC controller 
        if (SFR->AICREDIR == 0) {
            return SAPIC;
        }
    }
    return APIC;
}

/// Dummy handler for unconfigured interrupts
static void dummy_handler(void)
{
    asm volatile ("bkpt #0");
}

/// Initialize the APIC controller and redirect all secure interrupt to the 
/// non-secure interrupt controller
void apic_init(struct apic_reg* apic)
{
    for (u8 i = 1; i < PERIPH_CNT; i++) {
        apic->SSR = i;
        apic->IDCR = 1;
        apic->ICCR = 1;
    }

    // Acknowledge all the stacked interrupts 
    for (u8 i = 0; i < 8; i++) {
        apic->EOICR = 0;
    }

    for (u8 i = 0; i < PERIPH_CNT; i++) {
        apic->SSR = i;
        apic->SVR = (u32)dummy_handler;
    }
    apic->SPU = (u32)dummy_handler;
}

/// Enables protect mode. This mode should be used when a debugger is attatched.
/// Warning: this has concequences in the interrupt handler. Look at a 
/// implementation from Atmel. The solution is NOT specified in the datasheet...
void apic_protect(struct apic_reg* apic)
{
    apic->DCR = 1;
}

/// Configures the mode and interrupt handler of the selected interrupt source.
/// This functions also clears the status bit
void apic_irq_init(u8 irq, enum apic_pri pri,
    enum apic_event event, void (*func)(void))
{
    struct apic_reg* apic = get_apic(irq);

    apic->SSR = irq;
    apic->IDCR = 1;
    apic->SMR = pri | (event << 5);
    apic->SVR = (u32)func;
    apic->ICCR = 1;
}

void apic_enable(u8 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->IECR = 1;
}

void apic_disable(u8 irq)
{
    struct apic_reg* apic = get_apic(irq);
    apic->SSR = irq;
    apic->IDCR = 1;
}
