list(APPEND SOURCES
    alloc.c
    bcache.c
    bio.c
    dir.c
    ext2.c
    inode.c
    map.c
    node.c
)

add_driver(
    DRIVER_NAME ext2
    ARCHITECTURES ANY
    SOURCES ${SOURCES}
)
