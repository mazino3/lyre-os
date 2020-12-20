# Directories
REPODIR   := $(shell realpath .)

# Outputs
KERNEL    := ${REPODIR}/lyre.elf
IMAGE     := ${REPODIR}/lyre.hdd

# Compilers, several programs and their flags.
CXX  = g++
AS   = nasm
LD   = ld
QEMU = qemu-system-x86_64

CXXFLAGS  = -Wall -Wextra -O2
ASFLAGS   =
LDFLAGS   = -gc-sections -O2
QEMUFLAGS = -m 1G -smp 4 -debugcon stdio -enable-kvm -cpu host,+invtsc

CXXHARDFLAGS := ${CXXFLAGS} -std=c++11 -masm=intel -fno-pic -mno-80387 -mno-mmx \
    -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -ffreestanding \
    -fno-stack-protector -fno-omit-frame-pointer -fno-rtti -fno-exceptions
ASHARDFLAGS   := ${ASFLAGS} -felf64
LDHARDFLAGS   := ${LDFLAGS} -nostdlib -no-pie -z max-page-size=0x1000
QEMUHARDFLAGS := ${QEMUFLAGS}

# Source to compile.
CXXSOURCE := $(shell find ${REPODIR} -type f -name '*.cpp' | grep -v bootload)
ASMSOURCE := $(shell find ${REPODIR} -type f -name '*.asm' | grep -v bootload)
OBJ       := $(CXXSOURCE:.cpp=.o) $(ASMSOURCE:.asm=.o)

# Where the fun begins!
.PHONY: all test clean distclean

all: ${KERNEL}

${KERNEL}: ${OBJ}
	${LD} ${LDHARDFLAGS} ${OBJ} -T ${REPODIR}/linker.ld -o $@

%.o: %.cpp
	${CXX} ${CXXHARDFLAGS} -I${REPODIR} -c $< -o $@

%.o: %.asm
	${AS} ${ASHARDFLAGS} -I${REPODIR} $< -o $@

test: ${IMAGE}
	${QEMU} ${QEMUHARDFLAGS} -hda ${IMAGE}

${IMAGE}: limine ${KERNEL}
	dd if=/dev/zero bs=1M count=0 seek=64 of=${IMAGE}
	parted -s ${IMAGE} mklabel msdos
	parted -s ${IMAGE} mkpart primary 1 100%
	echfs-utils -m -p0 ${IMAGE} quick-format 32768
	echfs-utils -m -p0 ${IMAGE} import ${KERNEL} `basename ${KERNEL}`
	echfs-utils -m -p0 ${IMAGE} import limine.cfg limine.cfg
	limine/limine-install ${BOOTDIR}/limine/limine.bin ${IMAGE}

limine:
	git clone https://github.com/limine-bootloader/limine.git --depth=1 --branch=v0.7.2
	$(MAKE) -C limine limine-install

clean:
	rm -rf ${OBJ} ${KERNEL} ${IMAGE}

distclean: clean
	rm -rf limine
