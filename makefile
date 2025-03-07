#makefile for zenglOX

GCC_PATH = /Users/lee/Desktop/Computer_Systems/zenglOX-1/zenglOX_v0.0.1/opt/cross/bin
AS = @$(GCC_PATH)/i586-elf-as
CC = @$(GCC_PATH)/i586-elf-gcc

export CROSS_AS = $(AS)
export CROSS_CC = $(CC)
export CROSS_LD = @$(GCC_PATH)/i586-elf-ld
export CROSS_AR = @$(GCC_PATH)/i586-elf-ar
export CROSS_AS_FLAGS = -gstabs

#CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra
#CLINK_FLAGS = -ffreestanding -O2 -nostdlib
CFLAGS = -std=gnu99 -ffreestanding -gdwarf-2 -g3 -Wall -Wextra
CLINK_FLAGS = -ffreestanding -gdwarf-2 -g3 -nostdlib
ASFLAGS = -gdwarf-2 -g3

export CROSS_CFLAGS = $(CFLAGS)
export CROSS_CLINK_FLAGS = $(CLINK_FLAGS)
export RM = rm
export RMFLAGS = -rvf

DEPS = zlox_common.h zlox_monitor.h zlox_descriptor_tables.h zlox_isr.h zlox_time.h zlox_kheap.h \
		zlox_paging.h zlox_ordered_array.h zlox_initrd.h zlox_fs.h zlox_multiboot.h zlox_task.h \
		zlox_keyboard.h zlox_elf.h zlox_ata.h zlox_iso.h zlox_zenglfs.h zlox_vga.h zlox_pci.h \
		zlox_e1000.h zlox_network.h zlox_ps2.h zlox_uheap.h zlox_ide.h zlox_mouse.h zlox_term_font.h \
		zlox_my_windows.h zlox_sb16.h zlox_audio.h zlox_usb.h zlox_uhci.h \
		zlox_usb_mouse.h zlox_usb_hub.h zlox_usb_keyboard.h
OBJS = zlox_boot.o zlox_kernel.o zlox_common.o zlox_monitor.o zlox_descriptor_tables.o \
		zlox_gdt.o zlox_interrupt.o zlox_isr.o zlox_time.o zlox_kheap.o zlox_paging.o \
		zlox_ordered_array.o zlox_initrd.o zlox_fs.o zlox_task.o zlox_process.o zlox_syscall.o \
		zlox_keyboard.o zlox_elf.o zlox_shutdown.o zlox_ata.o zlox_iso.o zlox_zenglfs.o zlox_vga.o \
		zlox_pci.o zlox_e1000.o zlox_network.o zlox_ps2.o zlox_uheap.o zlox_ide.o zlox_mouse.o \
		zlox_my_windows.o zlox_sb16.o zlox_audio.o zlox_usb.o zlox_uhci.o \
		zlox_usb_mouse.o zlox_usb_hub.o zlox_usb_keyboard.o

INITRD_IMG = build_initrd_img/initrd.img

zenglOX.bin: $(OBJS) linker.ld $(INITRD_IMG)
	@echo 'building zenglOX.bin'
	$(CC) -T linker.ld -o zenglOX.bin $(CLINK_FLAGS) $(OBJS)

%.o: %.s
	@echo "building $@"
	$(AS) -o $@ $< $(ASFLAGS)

%.o: %.c $(DEPS)
	@echo "building $@"
	$(CC) -c -o $@ $< $(CFLAGS)

build_initrd_img/initrd.img:
	@(cd build_initrd_img; make)

initrd:
	@(cd build_initrd_img; make)

cleanrd:
	@(cd build_initrd_img; make clean)

clean:
	$(RM) $(RMFLAGS) *.o
	$(RM) $(RMFLAGS) zenglOX.bin
	$(RM) $(RMFLAGS) zenglOX.iso
	@(cd build_initrd_img; make clean)
	$(RM) $(RMFLAGS) isodir/boot/zenglOX.bin
	$(RM) $(RMFLAGS) isodir/boot/initrd.img
	$(RM) $(RMFLAGS) isodir/extra
	$(RM) $(RMFLAGS) isodir/boot/grub/grub.cfg
	$(RM) $(RMFLAGS) isodir/cpuid

all: zenglOX.bin

iso: zenglOX.bin
	cp zenglOX.bin isodir/boot/zenglOX.bin
	cp build_initrd_img/initrd.img isodir/boot/initrd.img
	cp build_initrd_img/cpuid isodir/cpuid
	mkdir -p isodir/extra
	cp build_initrd_img/extra/output/* isodir/extra/
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o zenglOX.iso isodir

