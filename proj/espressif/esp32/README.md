# Building and Uploading Bootloader
Apply the necessary patches
```
git -C lib/mcuboot apply ../../patches/mcuboot.patch
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

Apply required patches
```
git -C lib/mcuboot/boot/espressif/hal/esp-idf/ apply ../../../../../../patches/0001-support-idf-app-building-for-mcuboot-images.patch
```

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

Finally, to build and flash the application
```
cmake --build build --target app
cmake --build build --target mcuboot-app-flash
```

## Generating private key for signing application images
The bootloader expects images to be signed using user owned keys. Keys are critical in security. Add file protections to them
and do not distribute. For full details on image signing with MCUBoot, you may review their [documentation](https://github.com/mcu-tools/mcuboot/blob/main/docs/imgtool.md).

First, select a keypair scheme (rsa-2048, rsa-3072, ecdsa-p256, ed25519). This scheme must be consistent with the bootloader configuration. To designate, exclusively define related configs for the scheme in `boot/freertos/include/mcuboot_config/mcuboot_config.h`.
```c
// For RSA. Replace `YOUR_RSA_LEN` with the length of selected key.
#define MCUBOOT_SIGN_RSA
#define MCUBOOT_SIGN_RSA_LEN YOUR_RSA_LEN
```
Or
```c
// For ECDSA
#define MCUBOOT_SIGN_EC256
```

Generate the key using a scheme from the list above. In this example we'll use ecdsa-p256. Make sure to relocate/protect the key file adequately.
```console
pip install -r lib/mcuboot/scripts/requirements.txt
lib/mcuboot/scripts/imgtool.py keygen -k private-key.pem -t ecdsa-p256
```
## Generating public key for bootloader verification
The bootloader will use the associated public key to verify that the image was signed with your private key.

Generate the formatted public key from existing private key, and placing/replacing its output into `boot/freertos/keys.c`
```console
lib/mcuboot/scripts/imgtool.py getpub -k private-key.pem 
```

Finally, rebuild and upload the bootloader as described in previous sections.

## Signing and uploading application
Sign and format the image for bootloader
```console
cd proj/espressif/esp32/app
../../../../lib/mcuboot/scripts/imgtool.py sign --key private-key.pem --header-size 32 --align 4 --version 1.0 --slot-size 0x50000 --pad-header --pad build/app.bin build/signed-app.bin
```
Or, if the image was already padded with 0's at the beginning of the image, you can put:
```
../../../../lib/mcuboot/scripts/imgtool.py sign --key private-key.pem --header-size 32 --align 4 --version 1.0 --slot-size 0x100000 build/app.bin build/signed-app.bin
```

Define which USB port is connected to esp32 module. Replace `USB_PATH` with the path to your board USB descriptor. (Ex. /dev/ttyUSB1)
```console
export ESPPORT=${USB_PATH}
```

Flash the application to your board.
```console
$IDF_PATH/components/esptool_py/esptool/esptool.py -b 2000000 --before default_reset --after hard_reset --chip esp32 write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x20000 build/app.bin
```


# MCUMGR Interface

## Serial Firmware Updates
TODO: Show where to download the concluded mcumgr cli tool 
