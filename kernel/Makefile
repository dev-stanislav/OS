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
          -DBR_USE_URANDOM=0 \
          -DBR_USE_WIN32_RAND=0 \
          -DBR_USE_UNIX_TIME=0 \
          -DBR_USE_WIN32_TIME=0 \
          -DBR_RDRAND=0 \
          -DBR_AES_X86NI=0 \
          -DBR_SSE2=0 \
          -I./src \
          -I./third_party/bearssl/inc \
          -I./third_party/bearssl/src

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
	boot/idt_load.asm \
	src/interrupts.asm

KERNEL_C := \
	src/kernel.c \
	src/vga.c \
	src/gfx.c \
	src/mouse.c \
	src/gdt.c \
	src/idt.c \
	src/keyboard.c \
	src/console.c \
	src/timer.c \
	src/libk.c \
	src/bearssl_os.c \
	src/fs.c \
	src/disk.c \
	src/heap.c \
	src/paging.c \
	src/rtc.c \
	src/net.c \
	src/users.c \
	src/apps/app_manager.c \
	src/apps/minifetch.c \
	src/apps/calc.c \
	src/apps/clock.c \
	src/apps/files.c \
	src/apps/matrix.c \
	src/apps/mines.c \
	src/apps/free.c \
	src/apps/tbf.c \
	src/apps/paint.c \
	src/apps/terminal.c \
	src/apps/sproot.c

BEARSSL_C := \
	$(wildcard third_party/bearssl/src/aead/*.c) \
	$(wildcard third_party/bearssl/src/codec/*.c) \
	$(wildcard third_party/bearssl/src/ec/*.c) \
	$(wildcard third_party/bearssl/src/hash/*.c) \
	$(wildcard third_party/bearssl/src/int/*.c) \
	$(wildcard third_party/bearssl/src/kdf/*.c) \
	$(wildcard third_party/bearssl/src/mac/*.c) \
	$(filter-out third_party/bearssl/src/rand/sysrng.c,$(wildcard third_party/bearssl/src/rand/*.c)) \
	$(wildcard third_party/bearssl/src/rsa/*.c) \
	$(wildcard third_party/bearssl/src/ssl/*.c) \
	$(wildcard third_party/bearssl/src/symcipher/*.c) \
	$(wildcard third_party/bearssl/src/x509/*.c)

# Objects
BOOT_OBJ := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(BOOT_ASM))
KERNEL_OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C) $(BEARSSL_C))
ALL_OBJ := $(BOOT_OBJ) $(KERNEL_OBJ)

# Outputs
KERNEL := $(BUILD_DIR)/kernel
ISO_FILE := minios.iso
DISK_IMAGE := minios.img

.PHONY: all clean distclean iso run check help

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

$(DISK_IMAGE):
	@echo "Creating persistent disk: $@"
	dd if=/dev/zero of=$@ bs=1M count=4 status=none

run: all $(DISK_IMAGE)
	@command -v $(QEMU) >/dev/null 2>&1 || { echo "ERROR: qemu-system-i386 not found"; exit 1; }
	$(QEMU) \
		-vga std \
		-kernel $(KERNEL) \
		-drive file=$(DISK_IMAGE),format=raw,if=ide,index=0 \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
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

distclean: clean
	rm -f $(DISK_IMAGE)

help:
	@echo "Available commands:"
	@echo "  make        Build kernel"
	@echo "  make run    Build and run in QEMU"
	@echo "  make iso    Build GRUB ISO"
	@echo "  make clean  Delete generated files"
	@echo "  make distclean  Also delete the persistent disk"
