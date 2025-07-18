# Ethereal

A totally from scratch all-in-one operating system, written for speed, memory conservation, and reliability.\
Formerly known as reduceOS

## What is Ethereal?

Ethereal is a project with the goal of creating a fully functional OS from scratch with all components a modern OS would have.

Currently, the project is developing its usermode stages.

## Screenshots

<img width="1040" height="799" alt="image" src="https://github.com/user-attachments/assets/06835253-90cd-4785-a0f8-9f0042ac3676" />
*Ethereal main desktop environment*

![](https://preview.redd.it/ethereal-smp-enabled-usb-supporting-multitasking-kernel-v0-ygmgdlm3dowe1.png?width=1620&format=png&auto=webp&s=0473ac09024c17cefb294c8570671e415866b915)\
*Ethereal booting in debug mode*

![Ethereal but REALLY on IRC](https://github.com/user-attachments/assets/fedac2c5-db99-463f-88d9-44b1a239dcdd)\
*Ethereal on Libera chat (#ethereal)*

![](https://preview.redd.it/ethereal-smp-enabled-usb-supporting-multitasking-kernel-v0-ahe88a85dowe1.png?width=682&format=png&auto=webp&s=b435a1e0d57a91b7d4e0479f9649960b74a22de7)\
*Ethereal running DOOM*

![fixed](https://github.com/user-attachments/assets/bfdce356-5555-436b-990d-3aad65d22dea)\
*Ethereal running a neofetch clone made for it (whatarewe)*

## Features

- Full SMP-enabled kernel
- Custom window manager (Celestial)
- USB support for UHCI/EHCI/xHCI controllers
- AHCI/IDE support
- Networking stack with E1000 and RTL8139 network card driver
- Priority-based round-robin scheduler with a well-tested API
- Custom C library with support for many functions
- Full ACPI support with the ACPICA library

## Project structure

- `base`: Contains the base filesystem. Files in `base/initrd` go in the initial ramdisk (for non LiveCD boots) and files in `base/sysroot` go in sysroot.
- `buildscripts`: Contains buildscripts for the build system
- `conf`: Contains misc. configuration files, such as architecture files, GRUB configs, extra boot files, etc.
- `drivers`: Drivers for Hexahedron, copied based on their configuration.
- `external`: Contains external projects, such as ACPICA. See External Components.
- `hexahedron`: The main kernel project
- `libpolyhedron`: The libc/libk for the project.
- `libkstructures`: Contains misc. kernel structures, like lists/hashmaps/parsers/whatever

## Building

To build Ethereal, you will need an Ethereal toolchain for your target architecture.\
The Ethereal toolchain can be found at [the repository](https://github.com/sasdallas/Ethereal-Toolchain)

Other packages required: `grub-common`, `xorriso`, `qemu-system`

Edit `buildscripts/build-arch.sh` to change the target build architecture. \
Running `make all` will build an ISO in `build-output/ethereal.iso`

Currently, Ethereal's lack of filesystem drivers means that LiveCD boots are usually the best option.\
The initial ramdisk in a LiveCD is the sysroot, and if the OS detects the boot it will copy the initial ramdisk into RAM.

## Kernel arguments

A lot of times, Ethereal fails to load. This is expected. Please start a GitHub issue.

You can solve some problems by using 'e' to open a GRUB configuration and adding some kernel arguments to the end of the `multiboot entry`.\
Here is a small list:

- `--enable-ioapic`: Enable a prototype I/O APIC driver. Use this if you keyboard isn't working! (and you support PS/2)
- `--debug=`: Options are `console` and `none`. If `console`, will redirect kernel debug output to the screen. Useful for debugging
- `--noload=`: Comma-separated list of driver (.sys) files to not load. Problematic drivers: usb_xhci.sys, ahci.sys, ps2.sys (if you don't support PS/2),
- `--no-acpica`: Disable the ACPICA library and fallback to MinACPI implementation. Only useful in extreme cases.
- `--no-acpi`: Disable all ACPI implementations. Disables SMP as well.
- `--disable-smp`: Enable ACPI, but disable SMP
- `--no-secondary-fb`: Disables the secondary framebuffer. **RECOMMENDED** for real hardware, since double buffering is slow (**warning: scrolling will be extremely slow**)
- `--disable-cow`: Disable copy-on-write. Not recommended, but can be useful in extreme cases.
- `--xhci-fix-pci`: Enable xHCI PCI fix. Useful for Bochs.
- `--no-psf-font`: Don't load the PSF font from initrd

## External components
Certain external components are available in `external`. Here is a list of them and their versions:
- ACPICA UNIX* (Intel License): Version 20240927 [available here](https://www.intel.com/content/www/us/en/developer/topic-technology/open/acpica/download.html)
- libmusl math library (MIT License): [available here](https://github.com/kraj/musl)
- freetype (GPL license): [available here](https://sourceforge.net/projects/freetype/)

## Licensing

Hexahedron, libpolyhedron, and all other components of Ethereal fall under the terms of the BSD 3-clause license (available in LICENSE).\
All files, unless specified, fall under this license.


## Credits

The Ethereal logo and Mercury theme were designed by the artist [ArtsySquid](https://artsycomms.carrd.co)
