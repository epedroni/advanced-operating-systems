/*
 * Reference implementation of AOS milestone 0, on the Pandaboard.
 */

/*
 * Copyright (c) 2009-2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, Haldeneggsteig 4, CH-8092 Zurich. Attn: Systems Group.
 */

#include <kernel.h>

#include <assert.h>
#include <bitmacros.h>
#include <omap44xx_map.h>
#include <paging_kernel_arch.h>
#include <platform.h>
#include <serial.h>

#define MSG(format, ...) printk( LOG_NOTE, "OMAP44xx: "format, ## __VA_ARGS__ )

void blink_leds(void);

/* RAM starts at 2G (2 ** 31) on the Pandaboard */
lpaddr_t phys_memory_start= GEN_ADDR(31);

/*** Serial port ***/

unsigned serial_console_port= 2;

errval_t
serial_init(unsigned port, bool initialize_hw) {
    /* XXX - You'll need to implement this, but it's safe to ignore the
     * parameters. */

    return SYS_ERR_OK;
};

#define TX_FIFO_E (1 << 5) // FIFO Queue Empty
#define RX_FIFO_E (1 << 0) // FIFO Queue Empty

volatile uint32_t* UART_LSR = (volatile uint32_t*)0x48020014; // Contains status flags
volatile uint8_t* UART_THR = (volatile uint8_t*)0x48020000; // Where we write characters
void
serial_putchar(unsigned port, char c) {
     // Set it volatile as it is not only modified by CPU
     // Wait until it is free
     while (!(*UART_LSR & TX_FIFO_E));
     *UART_THR = c;
}

char
serial_getchar(unsigned port) {
    while (!(*UART_LSR & RX_FIFO_E));
    return char(*UART_THR & 0xFF);
}

/*** LED flashing ***/

#pragma GCC push_options
#pragma GCC optimize ("O0")
void homemade_wait(int timems);
void homemade_wait(int timems)
{
    for (int j = 0; j < timems; ++j)
        for (int i = 0; i < 100000; ++i)
        {

        }
}
#pragma GCC pop_options


void switch_gpio_out(unsigned int gpio_idx);

void blink_leds(void) {
    /* XXX - You'll need to implement this. */
    while (1)
    {
        switch_gpio_out(8);
        homemade_wait(1000);
    }
}

#define LED_OUTPUT_ENABLE_OFFSET  0x00000134
#define LED_DATAOUT_OFFSET        0x0000013C
void switch_gpio_out(unsigned int gpio_idx)
{
    static const uint32_t gpio_base[] =
    {
        0x4A310000,
        0x48055000,
        0x48057000,
        0x48059000,
        0x4805B000,
        0x4805D000,
    };
    int device_idx = gpio_idx / 32;
    int sub_idx = gpio_idx % 32;
    assert(device_idx >= 0 && device_idx < sizeof(gpio_base)/sizeof(gpio_base[0]));
    volatile uint32_t* output_enable = (volatile uint32_t*) (gpio_base[device_idx] + LED_OUTPUT_ENABLE_OFFSET);
    volatile uint32_t* dataout = (volatile uint32_t*) (gpio_base[device_idx] + LED_DATAOUT_OFFSET);
    *output_enable &= ~(1 << sub_idx);
    *dataout ^= (1 << sub_idx);
    //printf("gpio_%u [idx %u on device %u]: %s\n",
    //    gpio_idx, sub_idx, device_idx, *dataout & (1 << sub_idx) ? "ON" : "OFF");
}
