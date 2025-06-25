/**
 * @file libpolyhedron/include/sys/socket.h
 * @brief Socket address
 * 
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is apart of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2024 Samuel Stuart
 */

#include <sys/cheader.h>

_Begin_C_Header

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

/**** INCLUDES ****/
#include <stddef.h>
#include <sys/types.h> 

/**** DEFINITIONS ****/

/* Types of sockets */
#define SOCK_DGRAM          0       // Datagram socket
#define SOCK_RAW            1       // Raw protocol interface
#define SOCK_SEQPACKET      2       // Sequenced-packet stream
#define SOCK_STREAM         3       // Byte-stream socket

#define SOL_SOCKET          1

/* Socket options */
#define SO_ACCEPTCONN       0
#define SO_BROADCAST        1
#define SO_DEBUG            2
#define SO_DONTROUTE        3
#define SO_ERROR            4
#define SO_KEEPALIVE        5          
#define SO_LINGER           6
#define SO_OOBINLINE        7
#define SO_RCVBUF           8
#define SO_RCVLOWAT         9
#define SO_RCVTIMEO         10
#define SO_REUSEADDR        11
#define SO_SNDBUF           12
#define SO_SNDLOWAT         13
#define SO_SNDTIMEO         14
#define SO_TYPE             15
#define SO_BINDTODEVICE     16

/* Socket address families */
#define AF_INET             1       // IPv4
#define AF_INET6            2       // IPv6
#define AF_UNIX             3       // UNIX
#define AF_UNSPEC           4       // Unspecified
#define AF_RAW              5       // Raw

/* Shutdown types */
#define SHUT_RD             1
#define SHUT_RDWR           2
#define SHUT_WR             3

/* IP protocols */
/* TODO: Move to netinet/in.h */
#define IPPROTO_IP          0
#define IPPROTO_ICMP        1
#define IPPROTO_TCP         6
#define IPPROTO_UDP         17

/* Message flags */
#define MSG_CTRUNC          0x01
#define MSG_DONTROUTE       0x02
#define MSG_EOR             0x04
#define MSG_OOB             0x08
#define MSG_PEEK            0x10
#define MSG_TRUNC           0x20
#define MSG_WAITALL         0x40

/* PF */
#define PF_UNSPEC           AF_UNSPEC
#define PF_INET             AF_INET
#define PF_INET6            AF_INET6
#define PF_UNIX             AF_UNIX

/**** TYPES ****/
typedef size_t socklen_t;
typedef unsigned int sa_family_t;

typedef struct sockaddr {
    sa_family_t     sa_family;      // Address family
    char            sa_data[];      // Socket address  
};

typedef struct msghdr {
    void            *msg_name;          // Optional address
    socklen_t       msg_namelen;        // Size of address
    struct iovec    *msg_iov;           // Scatter/gather array
    int             msg_iovlen;         // Members in @c msg_iov
    void            *msg_control;       // Ancillary data
    socklen_t       msg_controllen;     // Ancillary data buffer length
    int             msg_flags;          // Flags on received message
};

struct sockaddr_storage {
    sa_family_t     ss_family;
    char            ss_storage[256];
};

struct linger {
    int     l_onoff;                    // Indicates whether linger option is enabled
    int     l_linger;                   // Linger time
};

/**** FUNCTIONS ****/

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int socket_vector[2]);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int socket, const struct sockaddr *addr, socklen_t addrlen);
int accept(int socket, struct sockaddr *address, socklen_t *addrlen);
int listen(int socket, int backlog);
ssize_t send(int socket, const void *buffer, size_t length, int flags);
ssize_t sendto(int socket, const void *message, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t dest_len);
ssize_t sendmsg(int socket, const struct msghdr *message, int flags);
ssize_t recv(int socket, void *buffer, size_t length, int flags);
ssize_t recvfrom(int socket, void *buffer, size_t length, int flags, struct sockaddr *address, socklen_t *address_len);
ssize_t recvmsg(int socket, struct msghdr *message, int flags);

int getpeername(int socket, struct sockaddr *address, socklen_t *address_len);
int getsockname(int socket, struct sockaddr *address, socklen_t *address_len);
int getsockopt(int socket, int level, int option_name, void *option_value, socklen_t *option_len);
int setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len);

int shutdown(int sockfd, int how);

#endif

_End_C_Header