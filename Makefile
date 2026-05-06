CC      = gcc
CFLAGS  = -m32 -ffreestanding -O2 -Wall -Wextra -fno-stack-protector
OBJS    = boot/boot.o \
          kernel/kernel.o kernel/memory.o kernel/history.o \
          kernel/gui.o kernel/auth.o kernel/shutdown.o \
          kernel/splash.o kernel/uptime.o kernel/sysinfo.o \
          kernel/calc.o kernel/banner.o kernel/session.o \
          drivers/vga.o drivers/keyboard.o drivers/rtc.o

all: myos.iso

boot/boot.o: boot/boot.asm
	nasm -f elf32 boot/boot.asm -o boot/boot.o

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel/kernel.o

kernel/memory.o: kernel/memory.c
	$(CC) $(CFLAGS) -c kernel/memory.c -o kernel/memory.o

kernel/history.o: kernel/history.c
	$(CC) $(CFLAGS) -c kernel/history.c -o kernel/history.o

kernel/gui.o: kernel/gui.c
	$(CC) $(CFLAGS) -c kernel/gui.c -o kernel/gui.o

kernel/auth.o: kernel/auth.c
	$(CC) $(CFLAGS) -c kernel/auth.c -o kernel/auth.o

kernel/shutdown.o: kernel/shutdown.c
	$(CC) $(CFLAGS) -c kernel/shutdown.c -o kernel/shutdown.o

kernel/splash.o: kernel/splash.c
	$(CC) $(CFLAGS) -c kernel/splash.c -o kernel/splash.o

kernel/uptime.o: kernel/uptime.c
	$(CC) $(CFLAGS) -c kernel/uptime.c -o kernel/uptime.o

kernel/sysinfo.o: kernel/sysinfo.c
	$(CC) $(CFLAGS) -c kernel/sysinfo.c -o kernel/sysinfo.o

kernel/calc.o: kernel/calc.c
	$(CC) $(CFLAGS) -c kernel/calc.c -o kernel/calc.o

kernel/banner.o: kernel/banner.c
	$(CC) $(CFLAGS) -c kernel/banner.c -o kernel/banner.o

kernel/session.o: kernel/session.c
	$(CC) $(CFLAGS) -c kernel/session.c -o kernel/session.o

drivers/vga.o: drivers/vga.c
	$(CC) $(CFLAGS) -c drivers/vga.c -o drivers/vga.o

drivers/keyboard.o: drivers/keyboard.c
	$(CC) $(CFLAGS) -c drivers/keyboard.c -o drivers/keyboard.o

drivers/rtc.o: drivers/rtc.c
	$(CC) $(CFLAGS) -c drivers/rtc.c -o drivers/rtc.o

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
