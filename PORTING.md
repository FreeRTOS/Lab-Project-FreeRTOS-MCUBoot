# Porting Guide

## Overview

The repo is structured for porting to other hardware. The provided `boot/freertos` makes use of the ports to provide the bootloader and serial interfacing with [`mcumgr`](https://github.com/apache/mynewt-mcumgr-cli).
To port, implement the functions in the port headers:
- `boot/freertos/flash_map_backend`
    - This is MCUBoot's typical porting layer. More info is provided in their [documentation](https://github.com/mcu-tools/mcuboot/blob/main/docs/PORTING.md).
- `boot/freertos/port`
    - This is specific to `boot/freertos`. A reference example is provided in this repo's `port` directory.

All the port functions require implementations, however a minority of those in `boot/freertos/port` can effectively be ommitted. See the **_Options_** section.
Finally, the application binary should be built with 32 zeroed bytes at the beginning -- easily achieved with the linker script. These are reserved and expected for MCUBoot's image signing tool `imgtool`.

## Options
The serial port `boot_serial_port.h`, responsible for interfacing [`mcumgr`](https://github.com/apache/mynewt-mcumgr-cli), can be omitted so long as `boot_port_serial_detect_boot_pin` always returns false.
Similarly the heap port `boot_heap_port.h` can always return NULL, so long as the bootloader is configured to use Tinycrypt, as specified in `mcuboot_config.h`.
