list(APPEND SOURCES
    rtl8169.c
)

add_driver(
    DRIVER_NAME net_rtl8169
    ARCHITECTURES ANY
    SOURCES ${SOURCES}
)
