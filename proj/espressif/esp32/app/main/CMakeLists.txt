set( BOOTLOADER_PORT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../../port/espressif/src )
set( MCUBOOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../../lib/mcuboot)
set( BOOTLOADER_DIR ${MCUBOOT_DIR}/boot/freertos/include )
set( IMGTOOL ${MCUBOOT_DIR}/scripts/imgtool.py )
set( ESPTOOL ${IDF_PATH}/components/esptool_py/esptool/esptool.py )

set( MCUBOOT_APP mcuboot-app )

if( NOT DEFINED APP_VERSION )
  set( APP_VERSION 1.0 )
endif()

if( NOT DEFINED PRIMARY_SLOT_ADDRESS )
  set( PRIMARY_SLOT_ADDRESS 0x20000 )
endif()

if( NOT DEFINED SECONDARY_SLOT_ADDRESS )
  set( SECONDARY_SLOT_ADDRESS 0x120000 )
endif()

if( NOT DEFINED IMAGE_SLOT_SIZE )
  set( IMAGE_SLOT_SIZE 0x100000 )
endif()

if( DEFINED SIGNING_SCHEME )
  if( NOT SIGNING_SCHEME MATCHES "ecdsa-p256" AND
      NOT SIGNING_SCHEME MATCHES "rsa-2048" AND 
      NOT SIGNING_SCHEME MATCHES "rsa-3072" )
    message( FATAL_ERROR "Valid SIGNING_SCHEME include: {ecdsa-p256, rsa-2048, rsa-3072}")
  endif()

  if( NOT DEFINED KEY_PATH OR NOT KEY_PATH MATCHES "^.*\.pem$" )
    message( FATAL_ERROR "KEY_PATH must be set to the same key used to build MCUBoot")
  else()
    get_filename_component( KEY_PATH "${KEY_PATH}" REALPATH BASE_DIR "${BUILD_DIR}/..")
  endif()

  message( STATUS "MCUBoot app signing scheme: ${SIGNING_SCHEME}" )
  message( STATUS "MCUBoot app signing key: ${KEY_PATH}" )
else()
  message( STATUS "MCUBoot app not configured for signing" )
endif()

message( STATUS "MCUBoot app version: ${APP_VERSION}" )
message( STATUS "MCUBoot app slot[0] address: ${PRIMARY_SLOT_ADDRESS}" )
message( STATUS "MCUBoot app slot[1] address: ${SECONDARY_SLOT_ADDRESS}" )
message( STATUS "MCUBoot app slot size: ${IMAGE_SLOT_SIZE}" )



idf_component_register(SRCS
                       start.c
                       main.c
                       ${BOOTLOADER_PORT_DIR}/boot_wdt_port.c
                       ${MCUBOOT_DIR}/boot/bootutil/src/bootutil_public.c
                       ${MCUBOOT_DIR}/boot/espressif/port/esp_mcuboot.c

                      INCLUDE_DIRS
                      ${IDF_PATH}/components/esp_system/port/include
                      ${BOOTLOADER_DIR}
                      ${MCUBOOT_DIR}/boot/bootutil/include
                      ${IDF_PATH}/components/bootloader_support/include_bootloader
)

if (DEFINED MCUBOOT_CONFIG_FILE)
    set(mcuboot_config_file ${MCUBOOT_CONFIG_FILE})
else()
    set(mcuboot_config_file "${CMAKE_CURRENT_LIST_DIR}/../../bootloader/bootloader.conf")
endif()

if (NOT EXISTS "${mcuboot_config_file}")
    message(FATAL_ERROR "MCUboot configuration file does not exist at ${mcuboot_config_file}")
endif()

configure_file(${mcuboot_config_file} dummy.conf)
file(STRINGS ${mcuboot_config_file} BOOTLOADER_CONF)
foreach(config ${BOOTLOADER_CONF})
    if (NOT (${config} MATCHES "#"))
        string(REGEX REPLACE "^[ ]+" "" config ${config})
        string(REGEX MATCH "^[^=]+" CONFIG_NAME ${config})
        string(REPLACE "${CONFIG_NAME}=" "" CONFIG_VALUE ${config})
        add_definitions(-D${CONFIG_NAME}=${CONFIG_VALUE})
    endif()
endforeach()

if( DEFINED SIGNING_SCHEME )
  add_custom_command( OUTPUT ${BUILD_DIR}/${MCUBOOT_APP}.bin
                      COMMAND ${IMGTOOL} sign --key ${KEY_PATH} --pad --header-size 32 --align 4 --version ${APP_VERSION} --slot-size ${IMAGE_SLOT_SIZE} ${BUILD_DIR}/app.bin ${BUILD_DIR}/${MCUBOOT_APP}.bin
                      WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/../..
                      DEPENDS ${BUILD_DIR}/app.bin ${KEY_PATH}
                      COMMENT "Signing mcuboot image: ${BUILD_DIR}/${MCUBOOT_APP}.bin"
                      VERBATIM
  )
  add_custom_target( mcuboot-app DEPENDS ${BUILD_DIR}/${MCUBOOT_APP}.bin )
else()
  add_custom_command( OUTPUT ${BUILD_DIR}/${MCUBOOT_APP}.bin
                      COMMAND ${IMGTOOL} sign --pad --header-size 32 --align 4 --version ${APP_VERSION} --slot-size ${IMAGE_SLOT_SIZE} ${BUILD_DIR}/app.bin ${BUILD_DIR}/${MCUBOOT_APP}.bin
                      WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                      DEPENDS ${BUILD_DIR}/app.bin
                      COMMENT "Formatting unsigned mcuboot image: ${BUILD_DIR}/${MCUBOOT_APP}.bin..."
                      VERBATIM
  )
  add_custom_target( mcuboot-app DEPENDS ${BUILD_DIR}/${MCUBOOT_APP}.bin )
endif()

add_custom_command( OUTPUT ${BUILD_DIR}/flash-stamp.txt
                    COMMAND ${ESPTOOL} -p $ENV{ESPPORT} -b 2000000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 40m ${PRIMARY_SLOT_ADDRESS} ${BUILD_DIR}/${MCUBOOT_APP}.bin
                    COMMAND date +%s > ${BUILD_DIR}/flash-stamp.txt
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                    DEPENDS ${BUILD_DIR}/${MCUBOOT_APP}.bin
                    COMMENT "Flashing mcuboot image at ${PRIMARY_SLOT_ADDRESS}: ${BUILD_DIR}/${MCUBOOT_APP}.bin..."
                    VERBATIM
)
add_custom_target( mcuboot-app-flash DEPENDS ${BUILD_DIR}/flash-stamp.txt )

add_custom_command( OUTPUT ${BUILD_DIR}/upgrade-stamp.txt
                    COMMAND ${ESPTOOL} -p $ENV{ESPPORT} -b 2000000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 40m ${SECONDARY_SLOT_ADDRESS} ${BUILD_DIR}/${MCUBOOT_APP}.bin
                    COMMAND date +%s > ${BUILD_DIR}/flash-stamp.txt
                    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
                    DEPENDS ${BUILD_DIR}/${MCUBOOT_APP}.bin
                    COMMENT "Upgrading mcuboot image at ${SECONDARY_SLOT_ADDRESS}: ${BUILD_DIR}/${MCUBOOT_APP}.bin..."
                    VERBATIM
)
add_custom_target( mcuboot-app-upgrade DEPENDS ${BUILD_DIR}/upgrade-stamp.txt )
