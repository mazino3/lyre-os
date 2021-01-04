#!/usr/bin/env bash

DEFAULT_IMAGE_SIZE=4096

set -e

if [ -z "$1" ]; then
    echo "Usage: ./bootstrap.sh BUILD_DIRECTORY [IMAGE_SIZE_MB]"
    exit 0
fi

# Accepted host OSes else fail.
OS=$(uname)
if ! [ "$OS" = "Linux" ] && ! [ "$OS" = "FreeBSD" ]; then
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
BUILD_DIR="$(realpath $1)"

# lyre kernel
LYRE_DIR="$(pwd)/kernel"

# Add toolchain to PATH
PATH="$BUILD_DIR/tools/host-binutils/bin:$PATH"
PATH="$BUILD_DIR/tools/host-gcc/bin:$PATH"

set -x

make -C "$LYRE_DIR" CC="x86_64-lyre-gcc"

if ! [ -f ./lyre.hdd ]; then
    dd if=/dev/zero bs=1M count=0 seek=$IMGSIZE of=lyre.hdd

    case "$OS" in
        "FreeBSD")
            sudo mdconfig -a -t vnode -f lyre.hdd -u md9
            sudo gpart create -s mbr md9
            sudo gpart add -a 4k -t '!14' md9
            sudo mdconfig -d -u md9
            ;;
        "Linux")
            parted -s lyre.hdd mklabel msdos
            parted -s lyre.hdd mkpart primary 1 100%
            ;;
    esac

    echfs-utils -m -p0 lyre.hdd quick-format 32768
fi

# Install limine
if ! [ -d limine ]; then
    git clone https://github.com/limine-bootloader/limine.git --depth=1 --branch=v1.0
    make -C limine
fi
limine/limine-install lyre.hdd

# Prepare root
install -m 644 $LYRE_DIR/lyre.elf root/
install -d root/etc
install -m 644 /etc/localtime root/etc/
install -d root/lib
install "$BUILD_DIR/system-root/usr/lib/ld.so" root/lib/

if [ "$USE_FUSE" = "no" ]; then
    ./copy-root-to-img.sh "$BUILD_DIR"/system-root lyre.hdd 0
    ./copy-root-to-img.sh root lyre.hdd 0
else
    mkdir -p mnt
    echfs-fuse --mbr -p0 lyre.hdd mnt
    while ! rsync -ru --copy-links --info=progress2 "$BUILD_DIR"/system-root/* mnt; do
        true
    done # FIXME: This while loop only exists because of an issue in echfs-fuse that makes it fail randomly.
    sync
    rsync -ru --copy-links --info=progress2 root/* mnt
    sync
    fusermount -u mnt/
    rm -rf ./mnt
fi
