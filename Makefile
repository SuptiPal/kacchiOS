CC = gcc
LD = ld
AS = as

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -nostdinc \
         -fno-builtin -fno-stack-protector -fno-pie \
         -fno-omit-frame-pointer -I.

ASFLAGS = --32
LDFLAGS = -m elf_i386

OBJS = boot.o kernel.o serial.o string.o \
       memory.o process.o scheduler.o ipc.o \
       interrupts.o interrupt_stubs.o timer.o

all: kernel.elf

kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -T link.ld -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(AS) $(ASFLAGS) $< -o $@

run: kernel.elf
	qemu-system-i386 -kernel kernel.elf -m 64M -serial stdio -display none

run-vga: kernel.elf
	qemu-system-i386 -kernel kernel.elf -m 64M -serial mon:stdio

debug: kernel.elf
	qemu-system-i386 -kernel kernel.elf -m 64M -serial stdio -display none -no-reboot -no-shutdown -s -S

clean:
	rm -f *.o kernel.elf

.PHONY: all run run-vga debug clean