# FreeRTOS+LwIP template project for EMW3165 #

This is a template project for building code for the EMW3165 using
FreeRTOS 8 and LwIP.  It is targeting the standard WiFiMCU development
board. It is probably possible to use with other EMW3165 hardware with
minor tweaks.

It uses a basic arm-none-eabi GCC toolchain to build, and OpenOCD to
upload code to the board. It uses the WiFi driver from Cypress' WICED
framework, but no other parts of it. It is otherwise self-contained and
has no other dependencies. It uses only the basic headers from the STM
libraries, as the STM driver code is generally quite atrocious.

## Toolchain ##

This code should build with any basic arm-none-eabi toolchain that has
support for the Cortex-M4 FPU, but it is developed using the official
ARM-maintained toolchain from https://launchpad.net/gcc-arm-embedded.

## Building ##

* `make` builds the `.elf` and `.bin` files for the project.

* `make upload` uses OpenOCD to talk to an STLink v2 programmer and uploads and runs the code on the board.

* `make debugserver` starts OpenOCD as a debug server, again using an STLink v2 programmer.

* `make debug` builds and connects to the debug server to upload and run the code.

* `make clean` cleans up all build product and dependency files.

For OS X users, there's a convenience Xcode project for code editing. Building should execute `make upload`.

## Installing WICED ##

As the WICED files are restrictively licensed, you will need to download them separtely
from Cypress. Only WICED-SDK-3.4.0-AWS is currently supported. You should download
`WICED-SDK-3.4.0-AWS.7z.zip` from Cypress' website, unpack it (twice), then put the
contained directories in the `WICED` directory.

Only the `include`, `libraries`, `resources` and `WICED` directories are actually used.

The `WICEDIncludes` directories contains symlinks to some files under the `WICED`
directory, for convenience.

## Notes ##

The WiFi chip driver uses the driver code from WICED, with a custom driver to talk
to the hardware (SDIO, IO pins and clocks). This is somewhat hackish, and some
corners were cut, for instance:

* The WICED code has a powersave mode for the WiFi chip. It is unclear if this works on the EMW3165, but it certainly doesn't work on with this driver.
* The only way to change the MAC address is to edit `WICEDIncludes/generated_mac_address.txt` by hand.
* The WiFi chip firmware data is stored in normal flash memory. This uses up a lot of space. It should be moved to the external flash, but this is not yet implemented.
* WICED contains lots of strange patches and changes to both FreeRTOS and LwIP. Most of them do not seem necessary, except for one small patch to `netif/etharp.c` in LwIP. I have tried to port this change to LwIP 1.4.1, but it does not apply entirely cleanly, nor do I fully understand it. It seems to work for now, though.

## License ##

Except for the FreeRTOS, LwIP, libopencm3, mbedtls and paho-mqtt parts, which
are released under their own license, this code is released into the public
domain with no warranties. If that is not suitable, it is also available under
the [CC0 license](http://creativecommons.org/publicdomain/zero/1.0/).

The WICED code is not distributed with this code, and can probably not be redistributed.
