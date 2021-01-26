.PHONY: all distro clean distclean run

all:
	MAKEFLAGS="$(MAKEFLAGS)" ./bootstrap.sh build/

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
    -m 2G           \
    -net none       \
    -debugcon stdio \
    -d cpu_reset    \
    -smp 5          \
    -hda lyre.hdd   \
    -enable-kvm -cpu host,+invtsc

run:
	qemu-system-x86_64 $(QEMU_FLAGS)
