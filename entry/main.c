/// Copyright (C) strawberryhacker 

#include <citrus-boot/types.h>
#include <citrus-boot/print.h>
#include <citrus-boot/boot.h>
#include <citrus-boot/packet.h>
#include <citrus-boot/kparam.h>
#include <citrus-boot/clk.h>
#include <citrus-boot/wdt.h>
#include <citrus-boot/apic.h>
#include <citrus-boot/ddr.h>
#include <citrus-boot/matrix.h>
#include <citrus-boot/bitops.h>
#include <citrus-boot/led.h>
#include <citrus-boot/host.h>

#define LOAD_ADDR 0x20000000

/// Initializes the components needed by citrus-boot
static void c_boot_init(void)
{
    // Disable watchdog 
    wdt_disable();

    // Reset the clock tree 
    clk_rst();

    // Set up the clock to running full speed 
    clk_rc_enable();
    clk_mainck_sel(CLK_SRC_RC);    // RC at 24 MHz 
    clk_plla_init(83, 0x3F, 1);    // PLLA at 498 MHz 

    /// Configure CPU clock to 498 MHz, master clock and H64MX clock to 166 MHz
    /// and the H32MX to 83 MHz
    clk_mck_init(MCK_SRC_PLLA_CLK, 1, MCK_PRESC_DISABLED, MCK_DIV_3);

    // Enable the DDR clock 
    clk_pck_enable(13);
    PMC->SCER = BIT(2);

    // Internal SRAM - one region - 128 kB - no split 
    matrix_set_sec(H64MX, SRAM, 0xFF, 0xFF, 0xFF);
    matrix_set_split(H64MX, SRAM, SPLIT_128K, 0);
    matrix_set_top(H64MX, SRAM, SPLIT_128K, 0);

    // External DDR2 all ports - one region - 128 MB - no split 
    for (u32 i = 0; i < 8; i++) {
        matrix_set_sec(H64MX, DDR_PORT0 + i, 0xFF, 0xFF, 0xFF);
        matrix_set_split(H64MX, DDR_PORT0 + i, SPLIT_128M, 0);
        matrix_set_top(H64MX, DDR_PORT0 + i, SPLIT_128M, 0);
    }

    // Initialize DDR2 RAM the MCK frequency must be greater than 125 MHz 
    ddr2_init();

    // Initialize the APIC 
    asm volatile ("cpsid if");
    apic_init(APIC);
    apic_protect(APIC); // Warning 
    apic_init(SAPIC);
    
    // Remape the secure APIC to non-secure 
    u32 key = SFR->SN1 ^ 0xB6D81C4D;
    SFR->AICREDIR = key | 1;

    // Temporary boot mode - to not burn fuses 
    *(volatile u32 *)0xF8045408 = (1 << 17) | (1 << 12);
    *(volatile u32 *)0xF8048054 = 0x66830000 | (1 << 2) | 2;

    // Initilaize hardware used by citrus-boot 
    led_init();
    print_init();
    host_init();

    asm volatile("cpsie if");

}

static void c_boot_deinit(void)
{
    asm volatile ("cpsid ifa");
}

/// Entry point after device spesific startup code
int main(void)
{
    c_boot_init();

    print("HELLO\n");

    while (1);

    // Make a function pointer to jump to the kernel 
    void (*kernel)(u32 load_addr) = (void(*)(u32))(LOAD_ADDR + 4);

    // Release all the resources except the kernel memory and clocks 
    c_boot_deinit();

    // No need for barriers since the kernel starts with the same stack pointer 
    kernel(0x20000000);
}
