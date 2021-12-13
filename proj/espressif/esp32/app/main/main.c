/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/image.h"

bool bReadImageHeader( struct image_header *pxImageHeader )
{
    /* Always read from primary slot */
    const struct flash_area *pFlashArea;
    if( flash_area_open( 1, &pFlashArea ) )
    {
        return false;
    }

    flash_area_read( pFlashArea, 0u, pxImageHeader, sizeof(struct image_header));
    flash_area_close( pFlashArea );
    return true;
}

void app_main(void)
{
    /* MCUBoot reverts to previous confirmed image, unless tentative image is confirmed too. */
    boot_set_confirmed();

    struct image_header xImageHeader = { 0 };
    bReadImageHeader( &xImageHeader );
    printf("Application Version: %d.%d.%d\n",
           xImageHeader.ih_ver.iv_major,
           xImageHeader.ih_ver.iv_minor,
           xImageHeader.ih_ver.iv_revision );

    while(1)
    {
        printf("Hello world!\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}