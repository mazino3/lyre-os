# Directories
REPODIR   := $(shell realpath .)

# Outputs
KERNEL    := ${REPODIR}/lyre.elf
IMAGE     := ${REPODIR}/lyre.hdd

# Compilers, several programs and their flags.
CC   = gcc
AS   = nasm

CFLAGS    = -Wall -Wextra -g -O2
ASFLAGS   = -F dwarf -g
LDFLAGS   = -O2

CHARDFLAGS := ${CFLAGS} -std=gnu11 -fplan9-extensions -masm=intel \
	-fno-pic -fpie \
    -mgeneral-regs-only \
    -mno-red-zone -ffreestanding \
    -fno-stack-protector
ASHARDFLAGS   := ${ASFLAGS} -felf64
LDHARDFLAGS   := ${LDFLAGS} \
	-nostdlib \
	-shared \
	-pie -fno-pic -fpie \
	-z max-page-size=0x1000
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
