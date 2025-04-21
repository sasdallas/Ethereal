# Ethereal

A totally from scratch all-in-one operating system, written for speed, memory conservation, and reliability.

## What is Ethereal?

Ethereal is a project with the goal of creating a fully functional OS with all components that a user requires.

Currently, the project is developing its usermode stages.

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
To build reduceOS, you will need a cross compiler for your target architecture.\
Other packages required: `grub-common`, `xorriso`, `qemu-system`

Edit `buildscripts/build-arch.sh` to change the target build architecture. \
Running `make all` will build an ISO in `build-output/reduceOS.iso`

## External components
Certain external components are available in `external`. Here is a list of them and their versions:
- ACPICA UNIX* (Intel License): Version 20240927 [available here](https://www.intel.com/content/www/us/en/developer/topic-technology/open/acpica/download.html)

## Licensing

Hexahedron, libpolyhedron, and all other components of Ethereal fall under the terms of the BSD 3-clause license (available in LICENSE).\
All files, unless specified, fall under this license.
