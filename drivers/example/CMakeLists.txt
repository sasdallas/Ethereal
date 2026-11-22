list(APPEND SOURCES
    main.c
)

add_driver(
    DRIVER_NAME example_driver
    ARCHITECTURES ANY
    SOURCES ${SOURCES}
)
