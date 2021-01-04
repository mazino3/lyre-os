# Directories
REPODIR   := $(shell realpath .)

# Outputs
KERNEL    := ${REPODIR}/lyre.elf
IMAGE     := ${REPODIR}/lyre.hdd

# Compilers, several programs and their flags.
CC   = gcc
AS   = nasm

CFLAGS    = -Wall -Wextra -O2
ASFLAGS   =
LDFLAGS   = -O2

CHARDFLAGS := ${CFLAGS} -std=gnu11 -fplan9-extensions -masm=intel -fno-pic -mno-80387 -mno-mmx \
    -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -ffreestanding \
    -fno-stack-protector -fno-omit-frame-pointer
ASHARDFLAGS   := ${ASFLAGS} -felf64
LDHARDFLAGS   := ${LDFLAGS} -nostdlib -no-pie -z max-page-size=0x1000
QEMUHARDFLAGS := ${QEMUFLAGS}

# Source to compile.
CSOURCE   := $(shell find ${REPODIR} -type f -name '*.c')
ASMSOURCE := $(shell find ${REPODIR} -type f -name '*.asm')
OBJ       := $(CSOURCE:.c=.o) $(ASMSOURCE:.asm=.o)

.PHONY: all run clean distclean

all: ${KERNEL}

${KERNEL}: ${OBJ}
	${CC} ${LDHARDFLAGS} ${OBJ} -T ${REPODIR}/linker.ld -o $@

%.o: %.c
	${CC} ${CHARDFLAGS} -I${REPODIR} -c $< -o $@

%.o: %.asm
	${AS} ${ASHARDFLAGS} -I${REPODIR} $< -o $@

clean:
	rm -rf ${OBJ} ${KERNEL} ${IMAGE}
