#include "port/boot_assert_port.h"

#include <stdlib.h>

extern int ets_printf( const char *fmt, ...);


void boot_port_assert_handler( const char *pcFile, int lLine, const char * pcFunc )
{
    ets_printf("assertion failed: file \"%s\", line %d, func: %s\n", pcFile, lLine, pcFunc);
    abort();
}