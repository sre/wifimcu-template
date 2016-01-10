# FreeRTOS+LwIP template project for EMW3165 #

This is a template project for building code for the EMW3165 using FreeRTOS 8 and LwIP.  It is targeting the standard WiFiMCU development board. It is probably possible to use with other EMW3165 hardware with minor tweaks.

It uses a basic arm-none-eabi GCC toolchain to build, and OpenOCD to upload code to the board. It uses the WiFi driver from Broadcom's WICED framework, but no other parts of it. It is otherwise self-contained and has no other dependencies. It uses only the basic headers from the STM libraries, as the STM driver code is generally quite atrocious.

## Toolchain ##

This code should build with any basic arm-none-eabi toolchain that has support for the Cortex-M4 FPU, but it is developed using the official ARM-maintained toolchain from https://launchpad.net/gcc-arm-embedded.

## Installing WICED ##

As the WICED files are restrictively licensed, you will need to download them separtely from Broadcom. Only WICED-SDK-3.4.0-AWS is currently supported. You should download `WICED-SDK-3.4.0-AWS.7z.zip` from Broadcom's website, unpack it (twice), then put the contained directories in the `WICED` directory.

Only the `WICED` and `libraries` folders are actually used.

## License ##

Except for the FreeRTOS and LwIP parts, which are released under their own license, this code is released into the public domain with no warranties. If that is not suitable, it is also available under the [CC0 license](http://creativecommons.org/publicdomain/zero/1.0/).

The WICED code is not distributed with this code, and can probably not be redistributed.
