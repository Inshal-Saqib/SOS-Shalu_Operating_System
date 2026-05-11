#!/bin/bash
set -e

echo "============================================"
echo "      MyOS - Setup & Build Script           "
echo "============================================"
echo ""

echo "[1/4] Installing dependencies..."
sudo apt update -qq
sudo apt install -y nasm gcc grub-pc-bin grub-common xorriso mtools qemu-system-x86 make
echo "      Done!"
echo ""

echo "[2/4] Assembling bootloader..."
nasm -f elf32 boot/boot.asm -o boot/boot.o

echo "[3/4] Compiling kernel..."
gcc -m32 -ffreestanding -O2 -Wall -fno-stack-protector -c kernel/kernel.c -o kernel/kernel.o
gcc -m32 -ffreestanding -O2 -Wall -fno-stack-protector -c drivers/vga.c -o drivers/vga.o

echo "      Linking..."
ld -m elf_i386 -T linker.ld --nostdlib \
    boot/boot.o kernel/kernel.o drivers/vga.o \
    $(gcc -m32 -print-libgcc-file-name) -o myos.bin

echo "[4/4] Building ISO..."
cp myos.bin iso/boot/myos.bin
grub-mkrescue -o myos.iso iso 2>/dev/null

echo ""
echo "============================================"
echo " SUCCESS! ->  myos.iso is ready"
echo "============================================"
echo ""
echo "Run in QEMU:       make run"
echo "Use in VMware:     copy myos.iso to Windows"
echo "Windows path:      \\\\wsl\$\\Ubuntu\\home\\$USER\\myos\\myos.iso"
