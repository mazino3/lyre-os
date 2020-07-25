# Some useful constants.
ARCH = x86_64

# Directories
REPODIR   := $(shell realpath ../../)
COMMONDIR := ${REPODIR}/common
ARCHDIR   := $(shell realpath .)
BOOTDIR   := ${ARCHDIR}/bootload

# Outputs
KERNEL    := ${REPODIR}/lyre.elf
IMAGE     := ${REPODIR}/lyre.hdd

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
CXXSOURCE := $(shell find ${COMMONDIR} ${ARCHDIR} -type f -name '*.cpp')
ASMSOURCE := $(shell find ${ARCHDIR} -type f -name '*.asm')
OBJ       := $(CXXSOURCE:.cpp=.o) $(ASMSOURCE:.asm=.o)

# Where the fun begins!
.PHONY: all test clean distclean

all: ${KERNEL}

${KERNEL}: ${OBJ}
	${LD} ${LDHARDFLAGS} ${OBJ} -T ${ARCHDIR}/linker.ld -o $@

%.o: %.cpp
	${CXXC} ${CXXHARDFLAGS} -I${ARCHDIR} -I${COMMONDIR} -c $< -o $@

%.o: %.asm
	${AS} ${ASHARDFLAGS} -I${ARCHDIR} -I${COMMONDIR} $< -o $@

test: ${IMAGE}
	${QEMU} ${QEMUHARDFLAGS} -hda ${IMAGE}

${IMAGE}: ${BOOTDIR}/qloader2 ${KERNEL}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${IMAGE}
	parted -s ${IMAGE} mklabel msdos
	parted -s ${IMAGE} mkpart primary 1 100%
	echfs-utils -m -p0 ${IMAGE} quick-format 32768
	echfs-utils -m -p0 ${IMAGE} import ${KERNEL} `basename ${KERNEL}`
	echfs-utils -m -p0 ${IMAGE} import ${BOOTDIR}/qloader2.cfg qloader2.cfg
	${BOOTDIR}/qloader2/qloader2-install ${BOOTDIR}/qloader2/qloader2.bin ${IMAGE}

${BOOTDIR}/qloader2:
	git clone https://github.com/qword-os/qloader2.git ${BOOTDIR}/qloader2

clean:
	rm -rf ${OBJ} ${KERNEL} ${IMAGE}

distclean: clean
	rm -rf ${BOOTDIR}/qloader2
