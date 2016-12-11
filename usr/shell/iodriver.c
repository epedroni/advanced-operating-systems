#include <aos/aos_rpc.h>
#include <aos/aos.h>
#include <aos/inthandler.h>
#include <omap44xx_map.h>
#include "shell.h"


#define TX_FIFO_E (1 << 5) // FIFO Queue Empty
#define RX_FIFO_E (1 << 0) // FIFO Queue Empty

#define OFFSET_THR 0x00
#define OFFSET_LSR 0x14

static void* uart_base_address = NULL;
volatile uint32_t* UART_LSR;
volatile uint8_t* UART_THR;

char input_buffer[30] = {0};
volatile int input_buffer_rpos = 0;
volatile int input_buffer_wpos = 0;

static void terminal_read_handler(void *params)
{
    if (!(*UART_LSR & RX_FIFO_E))
        return;
    char c = (char)(*UART_THR & 0xFF);
    if (c != 0)
    {
        input_buffer[input_buffer_wpos] = c;
        input_buffer_wpos = (input_buffer_wpos+1) % sizeof(input_buffer);
        //debug_printf("Got some char: %d [pos: %d %d]\n", c, input_buffer_rpos, input_buffer_wpos);
    }
}


void shell_getchar(char* c)
{
    terminal_read_handler((void*)0x1);
    if (input_buffer_rpos == input_buffer_wpos)
    {
        *c = 0;
        return;
    }
    char a = input_buffer[input_buffer_rpos];
    input_buffer_rpos = (input_buffer_rpos + 1) % sizeof(input_buffer);
    *c = a;
    return;
    aos_rpc_serial_getchar(get_init_rpc(), c);
    return;
    *c = input_buffer[input_buffer_rpos];
    input_buffer_rpos = (input_buffer_rpos + 1) % sizeof(input_buffer);
}

errval_t shell_setup_io_driver(void)
{
    struct capref io_driver_frame;
    ERROR_RET1(aos_rpc_get_special_capability(get_init_rpc(), AOS_CAP_IRQ, &cap_irq));
    ERROR_RET1(aos_rpc_get_special_capability(get_init_rpc(), AOS_CAP_IO_UART, &io_driver_frame));
    ERR_CHECK("mapping uart frame", paging_map_frame_attr(get_current_paging_state(), &uart_base_address,    //TODO: Send uart frame size!
            OMAP44XX_MAP_L4_PER_UART3_SIZE, io_driver_frame, VREGION_FLAGS_READ_WRITE | VREGION_FLAGS_NOCACHE, NULL, NULL));
    UART_THR = (volatile uint8_t*)(uart_base_address + OFFSET_THR);
    UART_LSR = (volatile uint32_t*)(uart_base_address + OFFSET_LSR);

    ERROR_RET1(inthandler_setup_arm(terminal_read_handler,
                         NULL,
                         106));
    return SYS_ERR_OK;
}

void shell_putchar(char c)
{
    aos_rpc_serial_putchar(get_init_rpc(), c);
}
