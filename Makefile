MCU ?= c8
ifeq ($(MCU),c6)
DEVICE_DEFINE := STM32F103x6
STARTUP := startup_stm32f103x6.s
else ifeq ($(MCU),c8)
DEVICE_DEFINE := STM32F103xB
STARTUP := startup_stm32f103xb.s
else
$(error Unsupported MCU '$(MCU)'. Choose c6 or c8)
endif

OBJDIR := build/$(MCU)
IMAGE := build/firmware-$(MCU)
LINKER_SCRIPT := stm32f103$(MCU).ld

CUBE_F1 ?= $(HOME)/STM32CubeF1
CMSIS := $(CUBE_F1)/Drivers/CMSIS
DEVICE := $(CMSIS)/Device/ST/STM32F1xx
TEMPLATES := $(DEVICE)/Source/Templates

CC := arm-none-eabi-gcc
ARCH := -mcpu=cortex-m3 -mthumb

# 1. Tự động tìm tất cả các thư mục chứa file .h trong các tầng kiến trúc AUTOSAR
SRC_DIRS := App board Common Services Mcal
INC_DIRS := $(shell find $(SRC_DIRS) -type d 2>/dev/null)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CPPFLAGS := $(INC_FLAGS) -I$(CMSIS)/Include -I$(DEVICE)/Include -D$(DEVICE_DEFINE) -DHSE_VALUE=8000000U
CFLAGS := $(ARCH) -std=c11 -Og -g3 -Wall -Wextra -ffunction-sections -fdata-sections -MMD -MP

CRTI := $(shell $(CC) $(ARCH) -print-file-name=crti.o)
CRTN := $(shell $(CC) $(ARCH) -print-file-name=crtn.o)

# 2. Tự động quét toàn bộ file .c trong dự án
C_SOURCES := $(shell find $(SRC_DIRS) -name '*.c' 2>/dev/null)
OBJECTS := $(addprefix $(OBJDIR)/,$(C_SOURCES:.c=.o)) $(OBJDIR)/system_stm32f1xx.o $(OBJDIR)/startup.o

vpath system_stm32f1xx.c $(TEMPLATES)

.PHONY: all clean compdb

all: $(IMAGE).elf $(IMAGE).bin

$(IMAGE).elf: $(OBJECTS) $(LINKER_SCRIPT) stm32f103_sections.ld Makefile
	$(CC) $(ARCH) -nostartfiles -T $(LINKER_SCRIPT) -Wl,--gc-sections,-Map=$(IMAGE).map $(CRTI) $(OBJECTS) $(CRTN) -o $@
	arm-none-eabi-size $@

$(IMAGE).bin: $(IMAGE).elf
	arm-none-eabi-objcopy -O binary $< $@

$(OBJDIR)/%.o: %.c Makefile | $(OBJDIR)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/startup.o: $(TEMPLATES)/gcc/$(STARTUP) Makefile | $(OBJDIR)
	$(CC) $(ARCH) -g3 -c $< -o $@

$(OBJDIR):
	mkdir -p $@

# 3. Dọn dẹp toàn bộ file build
clean:
	rm -rf build

# 4. Cập nhật database cho VS Code (clangd) khi thêm file/thư mục mới
compdb: clean
	bear -- make all MCU=$(MCU)

-include $(OBJECTS:.o=.d)