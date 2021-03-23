.PHONY: all distro clean distclean run

all:
	MAKEFLAGS="$(MAKEFLAGS)" ./bootstrap.sh build/
	cp lyre.hdd lyre.hdd.1

distro:
	mkdir -p build
	cd build && xbstrap init .. && xbstrap install --all

clean:
	$(MAKE) -C kernel clean
	rm -rf lyre.hdd

distclean: clean
	rm -rf build ports limine mlibc* initramfs.tar.gz

QEMU_FLAGS :=       \
    $(QEMU_FLAGS)   \
    -m 4G           \
    -net none       \
    -debugcon stdio \
    -d cpu_reset    \
    -smp 1          \
    -hda lyre.hdd   \
    -drive file=lyre.hdd.1,if=none,format=raw,id=NVME1 \
    -device nvme,drive=NVME1,serial=nvme-1 \
    -enable-kvm -cpu host,+invtsc

ovmf:
	mkdir -p ovmf
	cd ovmf && wget https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

run:
	qemu-system-x86_64 $(QEMU_FLAGS)

uefi-run:
	$(MAKE) ovmf
	qemu-system-x86_64 -L ovmf -bios ovmf/OVMF.fd $(QEMU_FLAGS)
