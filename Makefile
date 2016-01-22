NAME = Blinker

CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
GDB = arm-none-eabi-gdb
SIZE = arm-none-eabi-size
NM = arm-none-eabi-nm

#DEFINES = -DSTM32F407xx -DSTM32F4DISCOVERY
#DEFINES = -DSTM32F407xx -DSTM32F4DISCOVERY -DEnableOverclocking
#LINKERSCRIPT = Linker/Linker-STM32F407xG.ld
#INTERFACE = interface/stlink-v2.cfg

#DEFINES = -DSTM32F429xx -DSTM32F429IDISCOVERY
#LINKERSCRIPT = Linker/Linker-STM32F429xI.ld
#INTERFACE = interface/stlink-v2.cfg

#DEFINES = -DSTM32F446xx -DNUCLEO_F446RE
#LINKERSCRIPT = Linker/Linker-STM32F446xE.ld
#INTERFACE = interface/stlink-v2-1.cfg

DEFINES = -DSTM32F411xE -DWIFIMCU -DHSEFrequency=26000000
LINKERSCRIPT = Linker/Linker-STM32F411xE.ld
INTERFACE = interface/stlink-v2.cfg

C_OPTS =	-std=c99 \
			-mthumb \
			-mcpu=cortex-m4 \
			-mfloat-abi=hard \
			-mfpu=fpv4-sp-d16 \
			-IIncludes \
			-fdata-sections \
			-ffunction-sections \
			-g \
			-Werror \
			-Os

LIBS =	-lm

SOURCE_DIR = .
BUILD_DIR = Build

C_FILES =	Button.c \
			FormatString.c \
			LED.c \
			Main.c \
			Printf.c \
			Startup.c \
			System.c

S_FILES = 

OBJS = $(C_FILES:%.c=$(BUILD_DIR)/%.o) $(S_FILES:%.S=$(BUILD_DIR)/%.o)

ALL_CFLAGS = $(C_OPTS) $(DEFINES) $(CFLAGS)
ALL_LDFLAGS =	$(LD_FLAGS) \
				-mthumb \
				-mcpu=cortex-m4 \
				-mfloat-abi=hard \
				-mfpu=fpv4-sp-d16 \
				--specs=nosys.specs \
				-Wl,-T,$(LINKERSCRIPT),--gc-sections

AUTODEPENDENCY_CFLAGS=-MMD -MF$(@:.o=.d) -MT$@




all: $(NAME).bin

upload: $(NAME).bin
	openocd -f $(INTERFACE) -f target/stm32f4x.cfg \
	-c init -c "reset halt" -c "stm32f2x mass_erase 0" \
	-c "flash write_bank 0 $(NAME).bin 0" \
	-c "reset run" -c shutdown

debug: $(NAME).elf
	$(GDB) $(NAME).elf \
	--eval-command="target remote :3333" \
	--eval-command="load" \
	--eval-command="monitor reset init"

.PHONY: debugserver
debugserver:
	openocd -f $(INTERFACE) -f target/stm32f4x.cfg

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(NAME).elf $(NAME).bin

$(NAME).bin: $(NAME).elf
	$(OBJCOPY) -O binary $(NAME).elf $(NAME).bin

$(NAME).elf: $(OBJS)
	$(LD) $(ALL_LDFLAGS) -o $@ $^ $(LIBS)
	@$(SIZE) $@
	@printf "Binary size: "
	@$(SIZE) $@ | tail -n1 | awk '{print $$1+$$2}'
	@printf "Available heap: "
	@echo $$((0x`$(NM) $@ | grep _eheap | awk '{print $$1}'`))-$$((0x`$(NM) $@ | grep _heap | awk '{print $$1}'`)) | bc

.SUFFIXES: .o .c .S

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)

