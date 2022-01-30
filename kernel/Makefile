PREFIX ?= /boot

# Outputs
override KERNEL := lyre.elf

# Compilers, several programs and their flags.
ifeq ($(origin CC), default)
CC := gcc
endif
ifeq ($(origin AS), default)
AS := nasm
endif

CFLAGS ?= -Wall -Wextra -g -O2 -pipe
ASFLAGS ?= -F dwarf -g
LDFLAGS ?=

override CHARDFLAGS := \
	-I. \
	-std=gnu11 \
	-ffreestanding \
	-fplan9-extensions \
	-fno-pic \
	-fno-stack-protector \
	-fno-strict-aliasing \
	-mabi=sysv \
	-masm=intel \
	-mgeneral-regs-only \
	-mno-red-zone \
	-mcmodel=kernel \
	-MMD

override ASHARDFLAGS := \
	-I$(shell pwd) \
	-felf64

override LDHARDFLAGS := \
	-Tlinker.ld \
	-nostdlib \
	-z max-page-size=0x1000 \
	-static

# Source to compile.
override CSOURCE := $(shell find ./ -type f -name '*.c')
override ASMSOURCE := $(shell find ./ -type f -name '*.asm')
override OBJ := $(CSOURCE:.c=.o) $(ASMSOURCE:.asm=.o)
override HEADER_DEPS := $(CSOURCE:.c=.d)

.PHONY: all
all: $(KERNEL)

.PHONY: install
install:
	install -d "$(DESTDIR)$(PREFIX)"
	install -s $(KERNEL) "$(DESTDIR)$(PREFIX)/"

$(KERNEL): $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(LDHARDFLAGS) -o $@

-include $(HEADER_DEPS)
%.o: %.c
	$(CC) $(CFLAGS) $(CHARDFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $(ASHARDFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ) $(KERNEL) $(HEADER_DEPS)
