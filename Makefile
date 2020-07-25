# Some useful constants.
KERNEL    := lyre.elf
IMAGE     := lyre.hdd
CMAGENTA  := $(shell tput setaf 5)
CRESET    := $(shell tput sgr0)
SOURCEDIR := src
BUILDDIR  := build

# Compilers, several programs and their flags.
CXXC = g++
AS   = nasm
LD   = ld
QEMU = qemu-system-x86_64

CXXFLAGS  = -Wall -Wextra -O2
ASFLAGS   =
LDFLAGS   = -gc-sections -O2
QEMUFLAGS = -m 1G -smp 4 -debugcon stdio -enable-kvm -cpu host

CXXHARDFLAGS := ${CXXFLAGS} -std=c++11 -fno-pic -mno-sse -mno-sse2 -mno-mmx \
    -mno-80387 -mno-red-zone -mcmodel=kernel -ffreestanding -fno-stack-protector \
    -fno-omit-frame-pointer -fno-rtti -fno-exceptions
ASHARDFLAGS   := ${ASFLAGS} -felf64
LDHARDFLAGS   := ${LDFLAGS} -nostdlib -no-pie -z max-page-size=0x1000
QEMUHARDFLAGS := ${QEMUFLAGS}

# Source to compile.
CXXSOURCE := $(shell find ${SOURCEDIR} -type f -name '*.cpp')
ASMSOURCE := $(shell find ${SOURCEDIR} -type f -name '*.asm')
OBJ       := $(CXXSOURCE:.cpp=.o) $(ASMSOURCE:.asm=.o)

# Where the fun begins!
.PHONY: all test clean distclean

all: ${KERNEL}

${KERNEL}: ${OBJ}
	@echo "${CMAGENTA}${LD}${CRESET} $@"
	@${LD} ${LDHARDFLAGS} ${OBJ} -T ${BUILDDIR}/linker.ld -o $@

%.o: %.cpp
	@echo "${CMAGENTA}${CXXC}${CRESET} $@"
	@${CXXC} ${CXXHARDFLAGS} -I${SOURCEDIR} -c $< -o $@

%.o: %.asm
	@echo "${CMAGENTA}${AS}${CRESET} $@"
	@${AS} ${ASHARDFLAGS} -I${SOURCEDIR} $< -o $@

test: ${IMAGE}
	@${QEMU} ${QEMUHARDFLAGS} -hda ${IMAGE}

${IMAGE}: ${BUILDDIR}/qloader2 ${KERNEL}
	@dd if=/dev/zero bs=1M count=0 seek=64 of=${IMAGE}
	@parted -s ${IMAGE} mklabel msdos
	@parted -s ${IMAGE} mkpart primary 1 100%
	@echfs-utils -m -p0 ${IMAGE} quick-format 32768
	@echfs-utils -m -p0 ${IMAGE} import ${KERNEL} ${KERNEL}
	@echfs-utils -m -p0 ${IMAGE} import ${BUILDDIR}/qloader2.cfg qloader2.cfg
	@${BUILDDIR}/qloader2/qloader2-install ${BUILDDIR}/qloader2/qloader2.bin ${IMAGE}

${BUILDDIR}/qloader2:
	@git clone https://github.com/qword-os/qloader2.git ${BUILDDIR}/qloader2

clean:
	@rm -rf ${OBJ} ${KERNEL} ${IMAGE}

distclean: clean
	@rm -rf ${BUILDDIR}/qloader2
