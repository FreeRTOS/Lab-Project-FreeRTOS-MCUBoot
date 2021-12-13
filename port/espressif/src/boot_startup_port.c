#include "bootutil/bootutil.h"
#include "bootutil/image.h"
#include "bootutil/bootutil_log.h"
#include "port/boot_startup_port.h"

#include "esp_loader.h"
#include "bootloader_init.h"

void boot_port_init( void )
{
    bootloader_init();
}

/** Boots firmware at specific address */
void boot_port_startup( struct boot_rsp *rsp )
{
    MCUBOOT_LOG_INF("br_image_off = 0x%x", rsp->br_image_off);
    MCUBOOT_LOG_INF("ih_hdr_size = 0x%x", rsp->br_hdr->ih_hdr_size);
    int slot = (rsp->br_image_off == CONFIG_ESP_APPLICATION_PRIMARY_START_ADDRESS) ? 0 : 1;
    esp_app_image_load(slot, rsp->br_hdr->ih_hdr_size);
}