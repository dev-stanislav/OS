# MiniOS x86_64 Kernel

Простий 64-розрядний ядро операційної системи написане на асемблері та C.

## Вимоги

Для збирання цього проекту вам потрібно встановити наступне:

### На Linux/WSL:
```bash
# Встановлення інструментів для кросс-компіляції
sudo apt-get install build-essential nasm xorriso grub-pc-bin grub-efi-amd64-bin

# Встановлення i686-elf toolchain
sudo apt-get install gcc-multilib
# або скачати з https://github.com/osdev-kit/gcc-i686-elf-build
```

### На Windows:
1. Встановити WSL2 з Ubuntu
2. Відкрити WSL2 і виконати команди вище

## Функціональність

- ✅ GRUB Multiboot bootloader
- ✅ GDT (Global Descriptor Table) - управління сегментами
- ✅ IDT (Interrupt Descriptor Table) - каркас для обробки переривань
- ✅ VGA текстовий режим (80x25)
- ✅ Базовий виведення тексту з кольорами

## Будування

### 1. Зібрати ядро:
```bash
make all
```

### 2. Створити ISO файл:
```bash
make iso
```

Це створить файл `minios.iso` у поточній директорії.

### 3. Запустити з QEMU:
```bash
make run
```

Потім запустити:
```bash
qemu-system-x86_64 -cdrom minios.iso
```

Або одне команда:
```bash
qemu-system-i386 -cdrom minios.iso
```

## Структура проекту

```
kernel/
├── boot/
│   ├── boot.asm         - Точка входу, Multiboot header
│   ├── gdt_load.asm     - Завантаження GDT
│   ├── idt_load.asm     - Завантаження IDT
│   ├── linker.ld        - Скрипт компонування
│   └── grub.cfg         - Конфігурація GRUB
├── src/
│   ├── kernel.c         - Основна функція ядра
│   ├── vga.c/.h         - Функції VGA (виведення)
│   ├── gdt.c/.h         - Global Descriptor Table
│   └── idt.c/.h         - Interrupt Descriptor Table
├── Makefile             - Скрипт для збирання
└── README.md            - Цей файл
```

## Команди Make

```bash
make all    # Зібрати ядро
make iso    # Створити ISO файл
make run    # Показати команду для запуску з QEMU
make clean  # Видалити файли збирання
make help   # Показати цю довідку
```

## Готова команда для збирання ISO:

```bash
cd ~/kernel && make clean && make iso
```

Або якщо у вас вже є WSL/Linux:

```bash
make clean && make iso && qemu-system-i386 -cdrom minios.iso -m 512M
```

## Подальший розвиток

Ви можете додати:
- ☐ Обробка переривань (IRQ handlers)
- ☐ Клавіатура
- ☐ Таймер
- ☐ Простої системи файлів
- ☐ Багатозадачність
- ☐ Управління пам'яттю

## Посилання

- [OSDev.org Wiki](https://wiki.osdev.org/)
- [The Little OS Book](https://littleosbook.github.io/)
- [Bare Bones](https://wiki.osdev.org/Bare_Bones)
