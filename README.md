# STM32F4 Blinker #

This is a simple template project for building code for the STM32F4. It is a simple LED blinker, with Makefiles and linker scripts to get it up and running on any of the following cheap developer boards:

* STM32F4DISCOVERY
* 32F429IDISCOVERY
* NUCLEO-F446RE

It uses a basic arm-none-eabi GCC toolchain to build, and OpenOCD to upload code to the board. Beyond that, it has no dependencies. It uses only the basic headers from the STM libraries, as the STM driver code is generally quite atrocious.

## License ##

This code is released into the public domain with no warranties. If that is not suitable, it is also available under the [CC0 license](http://creativecommons.org/publicdomain/zero/1.0/).