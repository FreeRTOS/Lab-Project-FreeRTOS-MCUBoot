#include "port/system_port.h"
#include <endian.h>

#include "esp_rom_sys.h"
#include "esp32/rom/rtc.h"

uint16_t system_port_ntohs( uint16_t x )
{
    return be16toh( x );
}

uint16_t system_port_htons( uint16_t x )
{
    return htobe16( x );
}

void system_port_usleep( uint32_t usec )
{
    esp_rom_delay_us( usec );
}

void system_port_reset( void )
{
    software_reset();
}