#include "boot_serial/boot_serial.h"
#include "bootutil/bootutil_log.h"

#include "port/boot_serial_port.h"

#include <stdint.h>

#include "soc/rtc.h"
#include "soc/gpio_periph.h"
#include "soc/dport_reg.h"
#include "soc/uart_struct.h"
#include "soc/gpio_struct.h"
#include "esp32/rom/uart.h"
#include "esp32/rom/gpio.h"

#include "esp_rom_sys.h"
#include "esp_rom_uart.h"
#include "esp_rom_gpio.h"
#include "gpio_hal.h"
#include "gpio_ll.h"

#include "uart_ll.h"


#include <string.h>


#define BOOT_SERIAL_UART          ( 1 )
#define BOOT_SERIAL_UART_RX_PIN   ( GPIO_NUM_27 )
#define BOOT_SERIAL_UART_TX_PIN   ( GPIO_NUM_26 )
#define BOOT_SERIAL_UART_BAUDRATE ( 115200 )

#define BOOT_SERIAL_TRIGGER_BUTTON ( GPIO_NUM_5 )

#define BOOT_SERIAL_BUFF_SIZE ( 512 )


/***************************************************************************/
static int boot_port_serial_read( char * str, int cnt, int *newline );
static void boot_port_serial_write( const char *prt, int cnt );

static const struct boot_uart_funcs xUartFunctions = {
    .read = boot_port_serial_read,
    .write = boot_port_serial_write
};

const uint32_t GPIO_PIN_MUX_REG[SOC_GPIO_PIN_COUNT] = {
    IO_MUX_GPIO0_REG,
    IO_MUX_GPIO1_REG,
    IO_MUX_GPIO2_REG,
    IO_MUX_GPIO3_REG,
    IO_MUX_GPIO4_REG,
    IO_MUX_GPIO5_REG,
    IO_MUX_GPIO6_REG,
    IO_MUX_GPIO7_REG,
    IO_MUX_GPIO8_REG,
    IO_MUX_GPIO9_REG,
    IO_MUX_GPIO10_REG,
    IO_MUX_GPIO11_REG,
    IO_MUX_GPIO12_REG,
    IO_MUX_GPIO13_REG,
    IO_MUX_GPIO14_REG,
    IO_MUX_GPIO15_REG,
    IO_MUX_GPIO16_REG,
    IO_MUX_GPIO17_REG,
    IO_MUX_GPIO18_REG,
    IO_MUX_GPIO19_REG,
    0,
    IO_MUX_GPIO21_REG,
    IO_MUX_GPIO22_REG,
    IO_MUX_GPIO23_REG,
    0,
    IO_MUX_GPIO25_REG,
    IO_MUX_GPIO26_REG,
    IO_MUX_GPIO27_REG,
    0,
    0,
    0,
    0,
    IO_MUX_GPIO32_REG,
    IO_MUX_GPIO33_REG,
    IO_MUX_GPIO34_REG,
    IO_MUX_GPIO35_REG,
    IO_MUX_GPIO36_REG,
    IO_MUX_GPIO37_REG,
    IO_MUX_GPIO38_REG,
    IO_MUX_GPIO39_REG,
};
/***************************************************************************/

/* TODO: Should use the esp_rom api, but couldn't get it working */
static uint32_t readline( uint8_t * buf, uint32_t cap )
{
    volatile uint32_t len = 0;
    volatile uint32_t n_read = 0;
    volatile bool stop = false;

    static uart_dev_t * pxUART = &UART1;

    do
    {
        len = uart_ll_get_rxfifo_len( pxUART );

        for( uint32_t i=0; i<len; i++ )
        {
            uart_ll_read_rxfifo( pxUART, &buf[n_read], 1u );
            n_read++;

            /* Stop at capacity or once full packet is ingested */
            if( n_read == cap || buf[n_read-1] == '\n' )
            {
                stop = true;
            }
        }

        esp_rom_delay_us( 1000u );
    } while( !stop );


    return n_read;
}

static void configure_mcumgr_uart( void )
{
    /* Enable GPIO25 for UART1 RX */
    esp_rom_gpio_pad_select_gpio( BOOT_SERIAL_UART_RX_PIN );
    esp_rom_gpio_connect_in_signal (BOOT_SERIAL_UART_RX_PIN, uart_periph_signal[BOOT_SERIAL_UART].rx_sig, 0 );
    PIN_INPUT_ENABLE( GPIO_PIN_MUX_REG[BOOT_SERIAL_UART_RX_PIN] );
    esp_rom_gpio_pad_pullup_only( BOOT_SERIAL_UART_RX_PIN ); //x

    /* Enable GPIO26 for UART1 TX */
    esp_rom_gpio_pad_select_gpio( BOOT_SERIAL_UART_TX_PIN );
    esp_rom_gpio_connect_out_signal( BOOT_SERIAL_UART_TX_PIN, uart_periph_signal[BOOT_SERIAL_UART].tx_sig, 0, 0);
    uint32_t gpio_enable_out = REG_GET_FIELD(GPIO_ENABLE_W1TS_REG, GPIO_ENABLE_DATA_W1TS);
    gpio_enable_out = gpio_enable_out + (0x1 << BOOT_SERIAL_UART_TX_PIN);
    REG_WRITE(GPIO_ENABLE_W1TS_REG, gpio_enable_out);

    /* Configure BAUD */
    uart_ll_set_mode_normal( &UART1 );
    uart_ll_set_baudrate( &UART1, 115200  );
    uart_ll_set_stop_bits( &UART1, 1u );
    uart_ll_set_parity( &UART1, UART_PARITY_DISABLE );
    uart_ll_set_rx_tout( &UART1, 16 );

    /* Reset both fifos */
    uart_ll_txfifo_rst( &UART1 );
    uart_ll_rxfifo_rst( &UART1 );
    esp_rom_delay_us( 10 * 1000 );
}

static void configure_boot_serial_button( void )
{
    esp_rom_gpio_pad_select_gpio( BOOT_SERIAL_TRIGGER_BUTTON );
    PIN_INPUT_ENABLE( GPIO_PIN_MUX_REG[ BOOT_SERIAL_TRIGGER_BUTTON ] );
    gpio_ll_pulldown_en( &GPIO, BOOT_SERIAL_TRIGGER_BUTTON );
}


/***************************************************************************/
void boot_port_serial_init( void )
{
    configure_mcumgr_uart();
    configure_boot_serial_button();
}

bool boot_port_serial_detect_boot_pin( void )
{
    uint8_t aggregate = 0;
    const int n_samples = 5;
    for( int i=0; i<n_samples; i++ )
    {
        aggregate += gpio_ll_get_level( &GPIO, BOOT_SERIAL_TRIGGER_BUTTON );
        esp_rom_delay_us( 1000 );
    }

    return ( aggregate / n_samples ) == 1;
}

const struct boot_uart_funcs * boot_port_serial_get_functions( void )
{
    return &xUartFunctions;
}

static int boot_port_serial_read( char * str, int cnt, int *newline )
{
    volatile uint32_t n_read = 0;

    n_read = readline( (uint8_t *)str, cnt );
    *newline = (str[n_read-1] == '\n') ? 1 : 0;

    return n_read;
}

static void boot_port_serial_write( const char *prt, int cnt )
{
    uint32_t space = uart_ll_get_txfifo_len( &UART1 );
    if( cnt > space )
    {
        BOOT_LOG_ERR("Unable to send full message. TX fifo would overflow");
    }
    /* TODO: Should wait for available space, or continuously chunk */
    uart_ll_write_txfifo( &UART1, (const uint8_t *)prt, cnt );

    BOOT_LOG_DBG("Response[%d]: %s", cnt, prt);
}