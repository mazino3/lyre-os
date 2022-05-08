unexport CC
unexport CXX
unexport CFLAGS
unexport CXXFLAGS
unexport LDFLAGS
unexport MAKEFLAGS

.PHONY: all
all: lyre.iso

QEMUFLAGS = -M q35,smm=off -m 8G -cdrom lyre.iso -debugcon stdio

.PHONY: run-kvm
run-kvm: lyre.iso
	qemu-system-x86_64 -enable-kvm -cpu host $(QEMUFLAGS) -smp 4

.PHONY: run-hvf
run-hvf: lyre.iso
	qemu-system-x86_64 -accel hvf -cpu host $(QEMUFLAGS) -smp 4

ovmf:
	mkdir -p ovmf
	cd ovmf && curl -o OVMF-X64.zip https://efi.akeo.ie/OVMF/OVMF-X64.zip && 7z x OVMF-X64.zip

.PHONY: run-uefi
run-uefi: ovmf
	qemu-system-x86_64 -enable-kvm -cpu host $(QEMUFLAGS) -smp 4 -bios ovmf/OVMF.fd

.PHONY: run
run: lyre.iso
	qemu-system-x86_64 $(QEMUFLAGS) -no-shutdown -no-reboot -d int -smp 1

.PHONY: distro
distro:
	mkdir -p build 3rdparty
	cd build && [ -f bootstrap.link ] || xbstrap init ..
	cd build && xbstrap install -u --all

.PHONY: base-files
base-files: build
	cd build && xbstrap install --rebuild base-files

.PHONY: kernel
kernel:
	cd build && xbstrap install --rebuild kernel

lyre.iso: kernel base-files
	cd build && xbstrap run make-iso
	mv build/lyre.iso ./

.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -f lyre.iso

.PHONY: distclean
distclean: clean
	rm -rf 3rdparty build ovmf
	rm -f kernel/*.xbstrap
