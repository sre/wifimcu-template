# STM32F4 FreeRTOS+LwIP stack #

This is a simple template project for building code for the STM32F4 using FreeRTOS 8 and LwIP. It is a dummy project that only starts up the stack with no interface, and then shuts down. It has Makefiles and linker scripts to run (and do nothing) on these cheap developer boards:

* STM32F4DISCOVERY
* 32F429IDISCOVERY
* NUCLEO-F446RE
* WiFiMCU (EMW3165)

It uses a basic arm-none-eabi GCC toolchain to build, and OpenOCD to upload code to the board. Beyond that, it has no dependencies. It uses only the basic headers from the STM libraries, as the STM driver code is generally quite atrocious.

## Toolchain ##

This code should build with any basic arm-none-eabi toolchain that has support for the Cortex-M4 FPU, but it is developed using the official ARM-maintained toolchain from https://launchpad.net/gcc-arm-embedded.

## License ##

Except for the FreeRTOS and LwIP parts, which are released under their own license, this code is released into the public domain with no warranties. If that is not suitable, it is also available under the [CC0 license](http://creativecommons.org/publicdomain/zero/1.0/).
