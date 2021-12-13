cmake_minimum_required(VERSION 3.13)

set( ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../.. )
set( MCUBOOT_DIR ${ROOT_DIR}/lib/mcuboot )
set( MCUBOOT_FREERTOS_PORT_DIR ${MCUBOOT_DIR}/boot/freertos )
set( MCUBOOT_ESP_PORT_DIR ${MCUBOOT_DIR}/boot/espressif )
set( BOOTUTIL_DIR ${MCUBOOT_DIR}/boot/bootutil )
set( BOOTSERIAL_DIR ${MCUBOOT_DIR}/boot/boot_serial ) 
set( MBEDTLS_DIR ${MCUBOOT_DIR}/ext/mbedtls )
set( TINYCRYPT_DIR ${MCUBOOT_DIR}/ext/tinycrypt )
set( CMAKE_TOOLCHAIN_FILE ${MCUBOOT_DIR}/boot/espressif/tools/toolchain-esp32.cmake )
set( BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR} )
set( IMGTOOL ${MCUBOOT_DIR}/scripts/imgtool.py )

set( MCUBOOT_TARGET esp32 )
set( MCUBOOT_ARCH xtensa )
set( BOOTLOADER_ADDRESS 0x1000 )

if (NOT DEFINED IDF_PATH)
    if (EXISTS "${MCUBOOT_ESP_PORT_DIR}/hal/esp-idf")
        set(IDF_PATH "${MCUBOOT_ESP_PORT_DIR}/hal/esp-idf")
    elseif (DEFINED ENV{IDF_PATH})
        set(IDF_PATH $ENV{IDF_PATH})
    else()
        message(FATAL_ERROR "IDF_PATH not found. Please update submodules or set IDF_PATH environment variable or pass -DIDF_PATH flag.")
    endif()
endif()

set( ESPTOOL ${IDF_PATH}/components/esptool_py/esptool/esptool.py )



if (NOT DEFINED MCUBOOT_TARGET)
    message(FATAL_ERROR "MCUBOOT_TARGET not defined. Please pass -DMCUBOOT_TARGET flag.")
endif()

project(mcuboot_${MCUBOOT_TARGET})

add_definitions(-DMCUBOOT_TARGET=${MCUBOOT_TARGET})

execute_process(
    COMMAND git -C ${MCUBOOT_DIR} describe --tags
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE MCUBOOT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
add_definitions(-DMCUBOOT_VER=\"${MCUBOOT_VER}\")
message( STATUS "MCUBoot version: ${MCUBOOT_VER}" )

execute_process(
    COMMAND git describe --tags
    WORKING_DIRECTORY ${IDF_PATH}
    OUTPUT_VARIABLE IDF_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT "${IDF_VER}" MATCHES "v4.3")
    message(FATAL_ERROR "Unsupported ESP-IDF version found in IDF_PATH, please checkout to v4.3")
endif()

if (DEFINED MCUBOOT_CONFIG_FILE)
    set(mcuboot_config_file ${MCUBOOT_CONFIG_FILE})
else()
    set(mcuboot_config_file "${CMAKE_CURRENT_LIST_DIR}/bootloader.conf")
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

set(APP_NAME mcuboot_${MCUBOOT_TARGET})
set(APP_EXECUTABLE ${APP_NAME}.elf)

set(bootutil_srcs
    ${BOOTUTIL_DIR}/src/boot_record.c
    ${BOOTUTIL_DIR}/src/bootutil_misc.c
    ${BOOTUTIL_DIR}/src/bootutil_public.c
    ${BOOTUTIL_DIR}/src/caps.c
    ${BOOTUTIL_DIR}/src/encrypted.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening.c
    ${BOOTUTIL_DIR}/src/fault_injection_hardening_delay_rng_mbedtls.c
    ${BOOTUTIL_DIR}/src/image_ec.c
    ${BOOTUTIL_DIR}/src/image_ec256.c
    ${BOOTUTIL_DIR}/src/image_ed25519.c
    ${BOOTUTIL_DIR}/src/image_rsa.c
    ${BOOTUTIL_DIR}/src/image_validate.c
    ${BOOTUTIL_DIR}/src/loader.c
    ${BOOTUTIL_DIR}/src/swap_misc.c
    ${BOOTUTIL_DIR}/src/swap_move.c
    ${BOOTUTIL_DIR}/src/swap_scratch.c
    ${BOOTUTIL_DIR}/src/tlv.c
    )

set(bootserial_srcs
    ${BOOTSERIAL_DIR}/src/boot_serial.c
    ${BOOTSERIAL_DIR}/src/cbor_common.c
    ${BOOTSERIAL_DIR}/src/cbor_decode.c
    ${BOOTSERIAL_DIR}/src/cbor_encode.c
    ${BOOTSERIAL_DIR}/src/serial_recovery_cbor.c
)


set(mbedtls_srcs
    ${MBEDTLS_DIR}/library/sha256.c
    ${MBEDTLS_DIR}/library/platform_util.c
    ${MBEDTLS_DIR}/library/asn1parse.c
    ${MBEDTLS_DIR}/library/ecp.c
    ${MBEDTLS_DIR}/library/ecp_curves.c
    ${MBEDTLS_DIR}/library/bignum.c
    ${MBEDTLS_DIR}/library/ecdsa.c
    ${MBEDTLS_DIR}/library/base64.c
    ${MBEDTLS_DIR}/library/memory_buffer_alloc.c
    )

set(tinycrypt_srcs
    ${TINYCRYPT_DIR}/lib/source/ecc.c
    ${TINYCRYPT_DIR}/lib/source/ecc_dsa.c
    ${TINYCRYPT_DIR}/lib/source/sha256.c
    ${TINYCRYPT_DIR}/lib/source/utils.c
)

set(CFLAGS
    "-mlongcalls"
    "-Wno-frame-address"
    "-Wall"
    "-Wextra"
    "-W"
    "-Wdeclaration-after-statement"
    "-Wwrite-strings"
    "-Wlogical-op"
    "-Wshadow"
    "-ffunction-sections"
    "-fdata-sections"
    "-fstrict-volatile-bitfields"
    "-Werror=all"
    "-Wno-error=unused-function"
    "-Wno-error=unused-but-set-variable"
    "-Wno-error=unused-variable"
    "-Wno-error=deprecated-declarations"
    "-Wno-unused-parameter"
    "-Wno-sign-compare"
    "-ggdb"
    "-Os"
    "-D_GNU_SOURCE"
    "-DMBEDTLS_CONFIG=\"bootloader_mbedtls_config.h\""
    "-std=gnu99"
    "-Wno-old-style-declaration"
    "-Wno-implicit-int"
    "-Wno-declaration-after-statement"
    )

set(LDFLAGS
    "-nostdlib"
    "-mlongcalls"
    "-Wno-frame-address"
    "-Wl,--cref"
    "-Wl,--Map=${APP_NAME}.map"
    "-fno-rtti"
    "-fno-lto"
    "-Wl,--gc-sections"
    "-Wl,--undefined=uxTopUsedPriority"
    "-lm"
    "-lgcc"
    "-lgcov"
    "-lstdc++"
    "-lc"
    )
add_subdirectory(${MCUBOOT_ESP_PORT_DIR}/hal ${CMAKE_CURRENT_BINARY_DIR}/boot/espressif/hal)
add_executable(
    ${APP_EXECUTABLE}
    ${MCUBOOT_FREERTOS_PORT_DIR}/main.c
    ${MCUBOOT_FREERTOS_PORT_DIR}/keys.c
    )

target_compile_options(
    ${APP_EXECUTABLE}
    PUBLIC
    ${CFLAGS}
    )

include( port.cmake )

target_sources(
    ${APP_EXECUTABLE}
    PUBLIC
    ${bootutil_srcs}
    ${bootserial_srcs}
    ${mbedtls_srcs}
    ${tinycrypt_srcs}
    ${MCUBOOT_ESP_PORT_DIR}/port/esp_mcuboot.c
    ${MCUBOOT_ESP_PORT_DIR}/port/esp_loader.c
    ${freertos_mcuboot_sources}
    )

target_include_directories(
    ${APP_EXECUTABLE}
    PUBLIC
    ${BOOTUTIL_DIR}/include
    ${BOOTSERIAL_DIR}/include
    ${MBEDTLS_DIR}/include
    ${TINYCRYPT_DIR}/lib/include
    ${MCUBOOT_ESP_PORT_DIR}/include
    ${MCUBOOT_FREERTOS_PORT_DIR}/include
    ${freertos_mcuboot_includes}
    ${BUILD_DIR}
    .
    )

target_link_libraries(
    ${APP_EXECUTABLE}
    PUBLIC
    -T${CMAKE_CURRENT_LIST_DIR}/ld/bootloader.ld
    -T${CMAKE_CURRENT_LIST_DIR}/ld/rom_32.ld
    ${LDFLAGS}
    )

target_link_libraries(
    ${APP_EXECUTABLE}
    PUBLIC
    hal
    )


# TODO: This switches MCUBoot boot_serial to use my forks header paths (OSAL'ed)
target_compile_definitions(${APP_EXECUTABLE}
                            PUBLIC
                            __OTHER_PORT__=1)

if( DEFINED SIGNING_SCHEME )
    if( SIGNING_SCHEME MATCHES "ecdsa-p256" )
        set( GEN_KEY_H ecdsa_key.h)
        target_compile_definitions(${APP_EXECUTABLE}
                                    PUBLIC
                                    MCUBOOT_SIGN_EC256=1
                                    ECC256_KEY_FILE="ecdsa_key.h")
    elseif( SIGNING_SCHEME MATCHES "rsa-2048" )
        set( GEN_KEY_H ecdsa_key.h)
        target_compile_definitions(${APP_EXECUTABLE}
                                    PUBLIC
                                    MCUBOOT_SIGN_RSA=1
                                    MCUBOOT_SIGN_RSA_LEN=2048
                                    RSA_KEY_FILE="rsa_key.h")
    elseif( SIGNING_SCHEME MATCHES "rsa-3072" )
        set( GEN_KEY_H ecdsa_key.h)
        target_compile_definitions(${APP_EXECUTABLE}
                                    PUBLIC
                                    MCUBOOT_SIGN_RSA=1
                                    MCUBOOT_SIGN_RSA_LEN=3072
                                    RSA_KEY_FILE="rsa_key.h")
    else()
        message( FATAL_ERROR "Valid SIGNING_SCHEME include: {ecdsa-p256, rsa-2048, rsa-3072}")
    endif()
    
    message( STATUS "MCUBoot signing scheme: ${SIGNING_SCHEME}")

    if( NOT DEFINED KEY_PATH OR NOT KEY_PATH MATCHES "^.*\.pem$" )
        set( KEY_PATH ${CMAKE_CURRENT_LIST_DIR}/mcuboot-private-key.pem )
    endif()

                        

    add_custom_command( OUTPUT ${KEY_PATH}
                                COMMAND ${IMGTOOL} keygen -k ${KEY_PATH} -t ${SIGNING_SCHEME}
                                COMMENT "Generating signing keys: ${KEY_PATH}..."
                                VERBATIM
    )
    add_custom_command( OUTPUT ${BUILD_DIR}/${GEN_KEY_H}
                                COMMAND printf "/* Generated file. Do NOT edit manually.*/\\n" > ${BUILD_DIR}/${GEN_KEY_H}
                                COMMAND printf "\#ifndef _GEN_KEY_H_\\n\#define _GEN_KEY_H_\\n"  >> ${BUILD_DIR}/${GEN_KEY_H}
                                COMMAND ${IMGTOOL} getpub -k ${KEY_PATH} >> ${BUILD_DIR}/${GEN_KEY_H}
                                COMMAND printf "\#endif\\n"  >> ${BUILD_DIR}/${GEN_KEY_H}
                                DEPENDS ${KEY_PATH}
                                COMMENT "Generating signing header: ${BUILD_DIR}/${GEN_KEY_H}"
                                VERBATIM
    )
    add_custom_target( gen-keys DEPENDS ${BUILD_DIR}/${GEN_KEY_H})
    add_dependencies( ${APP_EXECUTABLE} gen-keys )
endif()


add_custom_command( OUTPUT ${BUILD_DIR}/mcuboot_esp32.bin
                            COMMAND ${ESPTOOL} --chip esp32 elf2image --flash_mode dio --flash_freq 40m -o ${BUILD_DIR}/mcuboot_esp32.bin ${BUILD_DIR}/mcuboot_esp32.elf
                            DEPENDS ${BUILD_DIR}/mcuboot_esp32.elf
                            COMMENT "Converting elf to binary"
                            VERBATIM
)

add_custom_target( mcuboot DEPENDS ${BUILD_DIR}/mcuboot_esp32.bin )


add_custom_command( OUTPUT ${BUILD_DIR}/flash-stamp.txt
                            COMMAND ${ESPTOOL} -p $ENV{ESPPORT} -b 2000000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 40m ${BOOTLOADER_ADDRESS} ${BUILD_DIR}/mcuboot_esp32.bin
                            COMMAND date +%s > ${BUILD_DIR}/flash-stamp.txt
                            DEPENDS ${BUILD_DIR}/mcuboot_esp32.bin
                            COMMENT "Flashing MCUBoot at ${BOOTLOADER_ADDRESS}..."
                            VERBATIM
)
add_custom_target( mcuboot-flash DEPENDS ${BUILD_DIR}/flash-stamp.txt )
