set( PORT_DIR ${ROOT_DIR}/port/espressif/src )


set( freertos_mcuboot_includes
    ${MCUBOOT_FREERTOS_PORT_DIR}/include
    ${MCUBOOT_ESP_PORT_DIR}/include
    ${BOOTUTIL_DIR}/include
    ${IDF_PATH}/components/driver/include
    ${IDF_PATH}/components/hal/include/hal
    ${IDF_PATH}/components/esp_system/include
    ${IDF_PATH}/components/hal/esp32/include/hal
    ${IDF_PATH}/components/newlib/platform_include
    ${IDF_PATH}/components/freertos/include
    ${IDF_PATH}/components/freertos/port/xtensa/include
    ${IDF_PATH}/components/esp_timer/include
)



set( freertos_mcuboot_sources
    ${PORT_DIR}/boot_startup_port.c
    ${PORT_DIR}/boot_log_port.c
    ${PORT_DIR}/boot_assert_port.c
    ${PORT_DIR}/boot_wdt_port.c
    ${PORT_DIR}/boot_heap_port.c
    ${PORT_DIR}/boot_serial_port.c
    ${PORT_DIR}/system_port.c
    ${PORT_DIR}/base64_port.c
    ${PORT_DIR}/crc16_port.c

    ${IDF_PATH}/components/soc/esp32/uart_periph.c
    #${IDF_PATH}/components/xtensa/xtensa_intr.c
    #${IDF_PATH}/components/newlib/syscalls.c
)
