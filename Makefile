CC      = gcc
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector
OBJS    = boot/boot.o kernel/kernel.o drivers/vga.o drivers/keyboard.o

all: myos.iso

boot/boot.o: boot/boot.asm
	nasm -f elf32 boot/boot.asm -o boot/boot.o

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel/kernel.o

drivers/vga.o: drivers/vga.c
	$(CC) $(CFLAGS) -c drivers/vga.c -o drivers/vga.o

drivers/keyboard.o: drivers/keyboard.c
	$(CC) $(CFLAGS) -c drivers/keyboard.c -o drivers/keyboard.o

myos.bin: $(OBJS)
	ld -m elf_i386 -T linker.ld --nostdlib $(OBJS) $(shell gcc -m32 -print-libgcc-file-name) -o myos.bin

myos.iso: myos.bin
	cp myos.bin iso/boot/myos.bin
	grub-mkrescue -o myos.iso iso

run: myos.iso
	qemu-system-i386 -cdrom myos.iso

clean:
	rm -f $(OBJS) myos.bin myos.iso iso/boot/myos.bin

.PHONY: all run clean
