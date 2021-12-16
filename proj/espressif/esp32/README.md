# Building and Uploading Bootloader
Apply the necessary patches

```console
user@pc$ git -C lib/mcuboot apply ../../patches/mcuboot.patch
user@pc$ git -C lib/mcuboot/boot/espressif/hal/esp-idf/ apply ../../../../../../patches/esp-idf.patch
```

Configure ESP-IDF tools
```console
user@pc$ ./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
user@pc$ source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
user@pc$ cd proj/espressif/esp32/bootloader
```

Define which USB port is connected to esp32 module. Ex.
```console
user@pc$ export ESPPORT=/dev/ttyUSB0
```

If you want cryptographic image verification, set `SIGNING_SCHEME` from the set {ecdsa-p256, rsa-2048, rsa-3072}, then generate build files i.e.
```console
user@pc$ cmake -GNinja -DSIGNING_SCHEME=ecdsa-p256 -B build
```
To omit crypto image verification, omit definition of `SIGNING_SCHEME` above. After the signing scheme is set, a private key will be generated for you in the `bootloader` directory. The private key will be used for signing the application images. Meanwhile, the cmake targets take care of embedding the public key in the bootloader, so it can verify the images.

Build and flash the bootloader
```console
user@pc$ cmake --build build --target mcuboot-flash
```

## Debugging The Bootloader
In two seperate shell sessions, configure ESP-IDF tools
```console
user@pc$ ./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
user@pc$ source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
```

In one terminal, enter the app directory and start the OpenOCD server
```console
user@pc$ cd proj/espressif/esp32/app
user@pc$ idf.py openocd
```

In the other terminal, enter the bootloader directory and start the GDB session
```console
user@pc$ cd proj/espressif/esp32/bootloader
user@pc$ xtensa-esp32-elf-gdb -x gdbinit build/mcuboot_esp32.elf
```

# Building and Uploading Application
The application first disables the bootloader watchdog timer, prints its version number, then confirms itself so it won’t be reverted if it's an update. The app proceeds to periodically print “hello world”. Follow the instructions below to build and flash the application.

Configure ESP-IDF tools
```console
user@pc$ ./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
user@pc$ source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
```

Define which USB port is connected to esp32 module. Ex.
```console
user@pc$ export ESPPORT=/dev/ttyUSB0
```

If you built the bootloader with a signing scheme, you must set the same scheme and specify the path to the aforementioned generated private key when generating build files. Ex.
```console
user@pc$ cd proj/espressif/esp32/app
user@pc$ cmake -DSIGNING_SCHEME=ecdsa-p256 -DKEY_PATH=../bootloader/mcuboot-private-key.pem -GNinja -B build
```
If the bootloader was not configured with image signature verification, you must omit the `SIGNING_SCHEME` and `KEY_PATH` definitions above.

Finally, build and flash the application directly to the primary image slot. This will overwrite the previous primary image.
```console
user@pc$ cmake --build build --target app
user@pc$ cmake --build build --target mcuboot-app-flash
```

Alternatively, you can _upgrade_ the application by downloading the image into the secondary image slot. If the the image in the secondary slot has a newer version than that of the primary slot, the bootloader will swap the images and tentatively boot the update image. Since the application confirms itself during this tentative boot, it then persists as primary image. To upgrade the image:
```console
user@pc$ cmake --build build --target mcuboot-app-upgrade
```
Finally, to view output from the device.

```console
user@pc$ idf.py monitor
```

## Debugging The Application
In two seperate shell sessions, configure ESP-IDF tools
```console
user@pc$ ./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
user@pc$ source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
```

In one terminal, enter the app directory and start OpenOCD server
```console
user@pc$ cd proj/espressif/esp32/app
user@pc$ idf.py openocd
```

In the other terminal, enter the app directory and start the GDB session
```console
user@pc$ cd proj/espressif/esp32/app
user@pc$ idf.py gdb
```

# MCUMGR Interface
Serial boot mode is enabled by default, but can be disabled by setting `MCUBOOT_SERIAL` to 0 in  `mcuboot_config.h` located in `lib/mcuboot/boot/freertos`. The serial boot pin is checked during boot up, and if active, serial boot mode is entered. For the demo, the serial boot pin is configured as `GPIO 5`, and is active high. You can ground the pin to skip serial mode, or tie it to VCC to enter serial mode. Once serial mode is running, you may interface the board with mcumgr. Installation instructions for mcumgr can be found here (https://github.com/apache/mynewt-mcumgr-cli).

MCUMGR will communicate with the board via UART pins which, for this demo, are set as `GPIO 27 (RX)` and `GPIO 26 (TX)`. You can use a USB-to-UART cable such as [this one](https://www.adafruit.com/product/954).

Define a connection for mcumgr setting the USB descriptor that corresponds with your USB-to-UART that connects your PC to the device.
```console
user@pc$ mcumgr conn add esp type="serial" connstring=“dev=/dev/<USB>,baud=115200,mtu=256”
```
To list images on the device
```console
user@pc$ mcumgr -c esp image list
```
To upload an image
```console
user@pc$ mcumgr -c esp image upload /path/to/mcuboot-image.bin
```
This expects a signed/formatted MCUBoot image. After building the application, this can be found in its build directory as `build/mcuboot-app.bin`. 

To reset the board 
```console
user@pc$ mcumgr -c esp reset
```

