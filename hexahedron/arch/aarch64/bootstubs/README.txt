aarch64 bootstubs
===============================================

This is a collection of "bootstubs", which load and initialize the kernel on different hardware systems.
The kernel itself is built separately and then passed in opt/org.ethereal.kernel - the kernel expects to be loaded at 0xFFFFF00000000000