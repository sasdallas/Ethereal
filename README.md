# Ethereal

A totally from scratch all-in-one operating system, written for speed, memory conservation, and reliability.\
Formerly known as reduceOS

## What is Ethereal?

Ethereal is a project with the goal of creating a fully functional OS from scratch with all components a modern OS would have.

Currently, the project is developing its usermode stages.

## Screenshots
![](https://preview.redd.it/ethereal-smp-enabled-usb-supporting-multitasking-kernel-v0-ygmgdlm3dowe1.png?width=1620&format=png&auto=webp&s=0473ac09024c17cefb294c8570671e415866b915)\
*Ethereal booting in debug mode*


![](https://preview.redd.it/ethereal-smp-enabled-usb-supporting-multitasking-kernel-v0-ahe88a85dowe1.png?width=682&format=png&auto=webp&s=b435a1e0d57a91b7d4e0479f9649960b74a22de7)\
*Ethereal running DOOM*


![](https://preview.redd.it/ethereal-smp-enabled-usb-supporting-multitasking-kernel-v0-hspq2m58dowe1.png?width=640&crop=smart&auto=webp&s=8764354d2764cb8bfaa77d1a79f710485880d780)\
*Ethereal running uname*

## Features
- Advanced driver system with a well-documented kernel API
- Strong support for x86_64 mode
- USB support for UHCI/EHCI controllers
- AHCI/IDE support
- Networking stack with E1000 network card driver
- Priority-based round-robin scheduler with a well-tested API
- Custom C library with support for many functions
- Full ACPI support with the ACPICA library

## Project structure
- `base`: Contains the base filesystem. Files in `base/initrd` go in the initial ramdisk and files in `base/sysroot` go in sysroot.
- `buildscripts`: Contains buildscripts for the build system
- `conf`: Contains misc. configuration files, such as architecture files, GRUB configs, extra boot files, etc.
- `drivers`: Drivers for Hexahedron, copied based on their configuration.
- `external`: Contains external projects, such as ACPICA. See External Components.
- `hexahedron`: The main kernel project
- `libpolyhedron`: The libc/libk for the project.
- `libkstructures`: Contains misc. kernel structures, like lists/hashmaps/parsers/whatever

## Building
To build Ethereal, you will need a cross compiler for your target architecture.\
Other packages required: `grub-common`, `xorriso`, `qemu-system`

Edit `buildscripts/build-arch.sh` to change the target build architecture. \
Running `make all` will build an ISO in `build-output/ethereal.iso`

## External components
Certain external components are available in `external`. Here is a list of them and their versions:
- ACPICA UNIX* (Intel License): Version 20240927 [available here](https://www.intel.com/content/www/us/en/developer/topic-technology/open/acpica/download.html)

## Licensing

Hexahedron, libpolyhedron, and all other components of Ethereal fall under the terms of the BSD 3-clause license (available in LICENSE).\
All files, unless specified, fall under this license.
