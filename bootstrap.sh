#!/usr/bin/env bash

DEFAULT_IMAGE_SIZE=4096

set -e

if [ -z "$1" ]; then
    echo "Usage: ./bootstrap.sh BUILD_DIRECTORY [IMAGE_SIZE_MB]"
    exit 0
fi

# Accepted host OSes else fail.
OS=$(uname)
if ! [ "$OS" = "Linux" ]; then
    echo "Host OS \"$OS\" is not supported."
    exit 1
fi

# Image size in MiB
if [ -z "$2" ]; then
    IMGSIZE=$DEFAULT_IMAGE_SIZE
else
    IMGSIZE="$2"
fi

# Make sure BUILD_DIR is absolute
BUILD_DIR="$(realpath "$1")"

# lyre kernel
LYRE_DIR="$(pwd)/kernel"

# Add toolchain to PATH
PATH="$BUILD_DIR/tools/host-binutils/bin:$PATH"
PATH="$BUILD_DIR/tools/host-gcc/bin:$PATH"

set -x

make -C "$LYRE_DIR" CC="x86_64-lyre-gcc"

mkdir -p "$BUILD_DIR/system-root/sbin"
x86_64-lyre-gcc -Ibuild/system-root/usr/include/libdrm -ldrm init/init.c -o "$BUILD_DIR/system-root/sbin/init"

if ! [ -f ./lyre.hdd ]; then
    dd if=/dev/zero bs=1M count=0 seek="$IMGSIZE" of=lyre.hdd
fi

# Install limine
if ! [ -d limine ]; then
    git clone https://github.com/limine-bootloader/limine.git --depth=1 --branch=v2.0-branch-binary
fi

# Prepare root
cp -r root/* "$BUILD_DIR"/system-root/

cp "$LYRE_DIR"/lyre.elf "$BUILD_DIR"/system-root/
cp limine/limine.sys "$BUILD_DIR"/system-root/
mkdir -p "$BUILD_DIR"/system-root/EFI/BOOT
cp limine/BOOTX64.EFI "$BUILD_DIR"/system-root/EFI/BOOT/
mkdir -p "$BUILD_DIR"/system-root/etc
cp /etc/localtime "$BUILD_DIR"/system-root/etc/

( cd "$BUILD_DIR"/system-root && tar -zcf ../../initramfs.tar.gz * )

parted -s lyre.hdd mklabel gpt
parted -s lyre.hdd mkpart primary 2048s 100%

rm -rf mnt
mkdir mnt
sudo losetup -Pf --show lyre.hdd > loopback_dev
sudo partprobe `cat loopback_dev`
sudo mkfs.fat -F 32 `cat loopback_dev`p1
sudo mount `cat loopback_dev`p1 mnt
sudo rsync -ru --copy-links --info=progress2 "$BUILD_DIR"/system-root/* mnt
sudo cp initramfs.tar.gz mnt/
sync
sudo umount mnt/
sudo losetup -d `cat loopback_dev`

limine/limine-install-linux-x86_64 lyre.hdd
