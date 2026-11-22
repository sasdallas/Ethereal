list(APPEND HEXAHEDRON_DRIVERS
    audio/hda
    example
    fs/ext2
    misc/ps2
    net/e1000
    net/rtl8169
    storage/ahci
    storage/ide
    storage/nvme
    usb/hub
    usb/keyboard
    usb/mouse
    usb/xhci
)

if(ENABLE_U80211)
    list(APPEND HEXAHEDRON_DRIVERS net/u80211)
endif()
