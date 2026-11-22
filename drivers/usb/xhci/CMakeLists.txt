list(APPEND SOURCES
    xhci.c
    xhci_pipe.c
    xhci_ring.c
    xhci_roothub.c
)

add_driver(
    DRIVER_NAME usb_xhci
    ARCHITECTURES X86_64
    SOURCES ${SOURCES}
)
