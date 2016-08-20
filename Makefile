NAME = Template

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

DEFINES =	-DSTM32F411xE \
			-DWIFIMCU \
			-DHSEFrequency=26000000 \
			-DLWIP_TIMEVAL_PRIVATE=0

LINKERSCRIPT = Linker/Linker-STM32F411xE.ld
INTERFACE = openocd-buspirate.cfg

C_OPTS =	-std=c99 \
			-mthumb \
			-mcpu=cortex-m4 \
			-mfloat-abi=hard \
			-mfpu=fpv4-sp-d16 \
			-IIncludes \
			-I. \
			-Ilibopencm3/include -DSTM32F4 \
			-Imbedtls/include \
			-IFreeRTOS/include \
			-IFreeRTOS/portable/GCC/ARM_CM4F \
			-ILwIP/src/include \
			-ILwIP/src/include/ipv4 \
			-ILwIP/port \
			-IWICEDIncludes \
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
			ws2812.c \
			Printf.c \
			Startup.c \
			System.c \
			WICEDPlatform.c \
			FreeRTOS/croutine.c \
			FreeRTOS/event_groups.c \
			FreeRTOS/list.c \
			FreeRTOS/queue.c \
			FreeRTOS/tasks.c \
			FreeRTOS/timers.c \
			FreeRTOS/portable/GCC/ARM_CM4F/port.c \
			FreeRTOS/portable/MemMang/heap_3.c \
			LwIP/src/api/api_lib.c \
			LwIP/src/api/api_msg.c \
			LwIP/src/api/err.c \
			LwIP/src/api/netbuf.c \
			LwIP/src/api/netdb.c \
			LwIP/src/api/netifapi.c \
			LwIP/src/api/sockets.c \
			LwIP/src/api/tcpip.c \
			LwIP/src/core/def.c \
			LwIP/src/core/dhcp.c \
			LwIP/src/core/dns.c \
			LwIP/src/core/init.c \
			LwIP/src/core/mem.c \
			LwIP/src/core/memp.c \
			LwIP/src/core/netif.c \
			LwIP/src/core/pbuf.c \
			LwIP/src/core/raw.c \
			LwIP/src/core/stats.c \
			LwIP/src/core/sys.c \
			LwIP/src/core/tcp.c \
			LwIP/src/core/tcp_in.c \
			LwIP/src/core/tcp_out.c \
			LwIP/src/core/timers.c \
			LwIP/src/core/udp.c \
			LwIP/src/core/ipv4/autoip.c \
			LwIP/src/core/ipv4/icmp.c \
			LwIP/src/core/ipv4/igmp.c \
			LwIP/src/core/ipv4/inet.c \
			LwIP/src/core/ipv4/inet_chksum.c \
			LwIP/src/core/ipv4/ip.c \
			LwIP/src/core/ipv4/ip_addr.c \
			LwIP/src/core/ipv4/ip_frag.c \
			LwIP/port/etharp.c \
			LwIP/port/sys_arch.c \
			mqtt/MQTTClient.c \
			mqtt/MQTTConnectClient.c \
			mqtt/MQTTConnectServer.c \
			mqtt/MQTTDeserializePublish.c \
			mqtt/MQTTFormat.c \
			mqtt/MQTTPacket.c \
			mqtt/MQTTPlatform.c \
			mqtt/MQTTSerializePublish.c \
			mqtt/MQTTSubscribeClient.c \
			mqtt/MQTTSubscribeServer.c \
			mqtt/MQTTUnsubscribeClient.c \
			mqtt/MQTTUnsubscribeServer.c

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

.PHONY: sizes
sizes: $(OBJS)
	$(SIZE) $(OBJS)

$(NAME).bin: $(NAME).elf
	$(OBJCOPY) -O binary $(NAME).elf $(NAME).bin

$(NAME).elf: $(OBJS) $(BUILD_DIR)/WICED.a $(BUILD_DIR)/mbedtls.a libopencm3/lib/libopencm3_stm32f4.a
	$(LD) $(ALL_LDFLAGS) -o $@ $^ $(LIBS)
	@$(SIZE) $@
	@printf "Binary size: "
	@$(SIZE) $@ | tail -n1 | awk '{print $$1+$$2}'
	@printf "Available heap: "
	@echo $$((0x`$(NM) $@ | grep _eheap | awk '{print $$1}'`))-$$((0x`$(NM) $@ | grep _heap | awk '{print $$1}'`)) | bc

.PHONY: libopencm3/lib/libopencm3_stm32f4.a
libopencm3/lib/libopencm3_stm32f4.a:
	cd libopencm3 && make

.PHONY: $(BUILD_DIR)/mbedtls.a
$(BUILD_DIR)/mbedtls.a:
	make -f Makefile.mbedtls

.PHONY: $(BUILD_DIR)/WICED.a
$(BUILD_DIR)/WICED.a:
	make -f Makefile.wiced

.SUFFIXES: .o .c .S

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) $(AUTODEPENDENCY_CFLAGS) -c $< -o $@

-include $(OBJS:.o=.d)


