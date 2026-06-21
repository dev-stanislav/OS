# MiniOS 32-bit x86 Kernel Makefile
# Environment: MSYS2 UCRT64
# Toolchain: Clang + LLD + NASM

CC := clang
LD := ld.lld
AS := nasm
QEMU := qemu-system-i386
GRUB_MKRESCUE := grub-mkrescue

# Compiler/linker flags
CFLAGS := --target=i686-none-elf \
          -m32 \
          -std=gnu11 \
          -ffreestanding \
          -fno-builtin \
          -fno-stack-protector \
          -fno-pic \
          -fno-pie \
          -mno-mmx \
          -mno-sse \
          -mno-sse2 \
          -Wall \
          -Wextra \
          -I./src

ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 \
           -T boot/linker.ld \
           -nostdlib

# Directories
BUILD_DIR := build
ISO_DIR := iso
ISO_BOOT_DIR := $(ISO_DIR)/boot
ISO_GRUB_DIR := $(ISO_BOOT_DIR)/grub

# Sources
BOOT_ASM := \
	boot/boot.asm \
	boot/gdt_load.asm \
	boot/idt_load.asm

KERNEL_C := \
	src/kernel.c \
	src/vga.c \
	src/gdt.c \
	src/idt.c \
	src/keyboard.c \
	src/console.c

# Objects
BOOT_OBJ := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(BOOT_ASM))
KERNEL_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C))
ALL_OBJ := $(BOOT_OBJ) $(KERNEL_OBJ)

# Outputs
KERNEL := $(BUILD_DIR)/kernel
ISO_FILE := minios.iso

.PHONY: all clean iso run check help

all: check $(KERNEL)

check:
	@command -v $(CC) >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
	@command -v $(LD) >/dev/null 2>&1 || { echo "ERROR: ld.lld not found"; exit 1; }
	@command -v $(AS) >/dev/null 2>&1 || { echo "ERROR: nasm not found"; exit 1; }

$(KERNEL): $(ALL_OBJ) boot/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJ)
	@echo
	@echo "Kernel built: $(KERNEL)"

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "ERROR: qemu-system-i386 not found"; exit 1; }
	$(QEMU) \
		-kernel $(KERNEL) \
		-m 128M \
		-no-reboot \
		-no-shutdown

iso: all
	@command -v $(GRUB_MKRESCUE) >/dev/null 2>&1 || { \
		echo "ERROR: grub-mkrescue not found."; \
		echo "Install GRUB tools, then run: make run"; \
		exit 1; \
	}
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_GRUB_DIR)
	cp $(KERNEL) $(ISO_BOOT_DIR)/kernel
	cp boot/grub.cfg $(ISO_GRUB_DIR)/grub.cfg
	$(GRUB_MKRESCUE) -o $(ISO_FILE) $(ISO_DIR)
	@echo
	@echo "ISO created: $(ISO_FILE)"

clean:
	rm -rf $(BUILD_DIR) $(ISO_DIR) $(ISO_FILE)

help:
	@echo "Available commands:"
	@echo "  make        Build kernel"
	@echo "  make run    Build and run in QEMU"
	@echo "  make iso    Build GRUB ISO"
	@echo "  make clean  Delete generated files"
