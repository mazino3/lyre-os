lyre
====

lyre is an x86 kernel and distribution powered by mlibc, GNU userland tools, and
other common *nix software.

Get Involved
============

Join our Discord server! https://discord.gg/2kdk3CbADg

Building
========

Build the distro first, which includes toolchain and everything lyre needs with
this command:

  make distro

Then build the kernel and image with:

  make

Try it in qemu with:

  make run

(it needs KVM because lyre depends on certain modern CPU features not emulated
by qemu)
