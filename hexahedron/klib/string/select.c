/**
 * @brief Code taken from mlibc
 */

#include <sys/select.h>
#include <string.h>
#include <assert.h>

void __FD_CLR(int fd, fd_set *set) {
    assert(fd >= 0 && fd < FD_SETSIZE);
    set->__fds_bits[fd / __NFDBITS] &= ~((unsigned long)1 << (fd % __NFDBITS));
}

int __FD_ISSET(int fd, fd_set *set) {
    assert(fd >= 0 && fd < FD_SETSIZE);
    return (set->__fds_bits[fd / __NFDBITS] & ((unsigned long)1 << (fd % __NFDBITS))) != 0;
}

void __FD_SET(int fd, fd_set *set) {
    assert(fd >= 0 && fd < FD_SETSIZE);
    set->__fds_bits[fd / __NFDBITS] |= (unsigned long)1 << (fd % __NFDBITS);
}

void __FD_ZERO(fd_set *set) {
	memset(set->__fds_bits, 0, sizeof(fd_set));
}
