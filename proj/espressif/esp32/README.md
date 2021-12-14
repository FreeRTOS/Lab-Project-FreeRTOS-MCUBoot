# Building and Uploading Bootloader
Apply the necessary patches
```
git -C lib/mcuboot apply ../../patches/mcuboot.patch
git -C lib/mcuboot/boot/espressif/hal/esp-idf/ apply ../../../../../../patches/esp-idf.patch
```

Configure ESP-IDF tools
```console
./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
cd proj/espressif/esp32/bootloader
```

Define which USB port is connected to esp32 module. Ex.
```console
export ESPPORT=/dev/ttyUSB0
```

If you want cryptographic image verification, set `SIGNING_SCHEME` from the set {ecdsa-p256, rsa-2048, rsa-3072}, then generate build files i.e.
```console
cmake -GNinja -DSIGNING_SCHEME=ecdsa-p256 -B build
```
To omit crypto image verification, don't define `SIGNING_SCHEME` above. After signing scheme is set, a private key will be generated for you in the `bootloader` directory. This same key will be used later, to sign the apps.

Build and flash the bootloader to the board
```console
cmake --build build --target mcuboot-flash
```

## Debugging Bootloader
Configure ESP-IDF tools
```console
./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
```

In a separate terminal, enter app directory, then start openocd server
```console
cd proj/espressif/esp32/app
idf.py openocd
```

In a separate terminal, enter bootloader directory, then start gdb session
```console
cd ../bootloader
xtensa-esp32-elf-gdb -x gdbinit build/mcuboot_esp32.elf
```

# Building and Uploading Application
The application marks confirms its image, so that it won't be reverted if its an update, prints the application version, then creates a task which periodically prints "hello world".

Configure ESP-IDF tools
```console
./lib/mcuboot/boot/espressif/hal/esp-idf/install.sh
source lib/mcuboot/boot/espressif/hal/esp-idf/export.sh
```

Define which USB port is connected to esp32 module. Ex.
```console
export ESPPORT=/dev/ttyUSB0
```

If you built the bootloader with a signing scheme, you must set the same scheme and specify the path to aforementioned generated private key when generating build files. Ex.
```console
cd proj/espressif/esp32/app
cmake -DSIGNING_SCHEME=ecdsa-p256 -DKEY_PATH=../bootloader/mcuboot-private-key.pem -GNinja -B build
```
If did not build the bootloader for image signature verification, you must omit the `SIGNING_SCHEME` and `KEY_PATH` definitions above.

Finally, to build and flash the application directly the primary image slot. This will overwrite the previous primary image.
```
cmake --build build --target app
cmake --build build --target mcuboot-app-flash
```

Alternatively, you can _upgrade_ the application by downloading the image into the secondary image slot. If the the image in the secondary slot has a newer version than that of the primary slot, the bootloader will swap the images and tentatively boot the update image. Since the application confirms itself during this tentative boot, it then persists as primary image. To upgrade the image:
```
cmake --build build --target mcuboot-app-upgrade
```

# MCUMGR Interface

## Serial Firmware Updates
TODO: Show where to download the concluded mcumgr cli tool 
