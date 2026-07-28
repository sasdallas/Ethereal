/**
 * @brief Code taken from mlibc
 */

#include <sys/select.h>
#include <string.h>
#include <assert.h>

void __FD_CLR(int fd, fd_set *set) {
	assert(fd < FD_SETSIZE);
	set->__fds_bits[fd / 8] &= ~(1 << (fd % 8));
}
int __FD_ISSET(int fd, fd_set *set) {
	assert(fd < FD_SETSIZE);
	return set->__fds_bits[fd / 8] & (1 << (fd % 8));
}
void __FD_SET(int fd, fd_set *set) {
	assert(fd < FD_SETSIZE);
	set->__fds_bits[fd / 8] |= 1 << (fd % 8);
}
void __FD_ZERO(fd_set *set) {
	memset(set->__fds_bits, 0, sizeof(fd_set));
}