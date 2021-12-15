# Labs-FreeRTOS-Plus-MCUBoot
Reference examples for FreeRTOS with [MCUBoot](https://github.com/mcu-tools/mcuboot) support.

MCUBoot is a configurable secure bootloader maintained by several industry leaders. It can operate as the first or second stage bootloader, with support for cryptographic verification of software images via one of the following:

* ECDSA-P256
* RSA-2048
* RSA-3072

By default, it supports image reversion whereby uploaded image upgrades are tentatively booted once. Upon and image's initial boot, if the upgrade image marks itself as confirmed it is retained as the primary image. If the upgrade image is not confirmed, the subsequent boot will rollback to the prior confirmed image. If no valid image is available in any slot, the device bricks itself as a safety precaution. The developers of MCUBoot have more detailed documentation here (https://github.com/mcu-tools/mcuboot/tree/main/docs).

MCUBoot also provides subset support for [`mcumgr`](https://github.com/apache/mynewt-mcumgr-cli) when a device enters serial boot recovery mode. If enabled, serial mode can be triggered during bootup via user input, such as a button hold. MCUMGR interface enables users to retrieve image diagnostics from the board, query resets, upload/modify images, and more.


| Supported Boards |
| ---- |
| Espressif esp32 |

## License

This library is licensed under the MIT-0 License. See the LICENSE file.

