NAME = Blinker

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

#DEFINES = -DSTM32F407xx
#DEFINES = -DSTM32F407xx -DEnableOverclocking
#LINKERSCRIPT = Linker-STM32F407xG.ld

#DEFINES = -DSTM32F429xx
#LINKERSCRIPT = Linker-STM32F429xI.ld

#DEFINES = -DSTM32F446xx
#LINKERSCRIPT = Linker-STM32F446xE.ld

DEFINES = -DSTM32F411xE -DHSEFrequency=26000000
LINKERSCRIPT = Linker-STM32F411xE.ld

C_OPTS =	-std=c99 \
			-mthumb \
			-mcpu=cortex-m4 \
			-IIncludes \
			-g \
			-Werror \
			-O0

LIBS =	-lm

SOURCE_DIR = .
BUILD_DIR = Build

C_FILES =	Button.c \
			LED.c \
			Main.c \
			Startup.c \
			System.c

S_FILES = 

OBJS = $(C_FILES:%.c=$(BUILD_DIR)/%.o) $(S_FILES:%.S=$(BUILD_DIR)/%.o)

ALL_CFLAGS = $(C_OPTS) $(DEFINES) $(CFLAGS)
ALL_LDFLAGS = $(LD_FLAGS) -mthumb -mcpu=cortex-m4 -nostartfiles -Wl,-T,$(LINKERSCRIPT),--gc-sections

AUTODEPENDENCY_CFLAGS=-MMD -MF$(@:.o=.d) -MT$@




all: $(NAME).bin

upload: $(NAME).bin
	openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
	-c init -c "reset halt" -c "stm32f2x mass_erase 0" \
	-c "flash write_bank 0 $(NAME).bin 0" \
	-c "reset run" -c shutdown

upload2: $(NAME).bin
	openocd -f interface/stlink-v2-1.cfg -f target/stm32f4x.cfg \
	-c init -c "reset halt" -c "stm32f2x mass_erase 0" \
	-c "flash write_bank 0 $(NAME).bin 0" \
	-c "reset run" -c shutdown

debug:
	arm-eabi-gdb $(NAME).elf \
	--eval-command="target remote | openocd -f interface/stlink-v2.cfg -f target/stm32f4x_stlink.cfg -c 'gdb_port pipe'"

stlink:
	arm-eabi-gdb $(NAME).elf --eval-command="target ext :4242"

clean:
	rm -rf $(BUILD_DIR) $(NAME).elf $(NAME).bin

$(NAME).bin: $(NAME).elf
	$(OBJCOPY) -O binary $(NAME).elf $(NAME).bin

$(NAME).elf: $(OBJS)
	$(LD) $(ALL_LDFLAGS) -o $@ $^ $(LIBS)

.SUFFIXES: .o .c .S

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

