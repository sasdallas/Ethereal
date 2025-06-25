/**
 * @file libpolyhedron/include/netdb.h
 * @brief Network database
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

#ifndef _NETDB_H
#define _NETDB_H

/**** INCLUDES ****/
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**** DEFINITIONS ****/

#define AI_PASSIVE          0x01
#define AI_CANONNAME        0x02
#define AI_NUMERICHOST      0x04
#define AI_NUMERICSERV      0x08
#define AI_V4MAPPED         0x10
#define AI_ALL              0x20
#define AI_ADDRCONFIG       0x40

#define HOST_NOT_FOUND      1
#define NO_DATA             2
#define NO_RECOVERY         3
#define TRY_AGAIN           4

#define NI_NOFQDN           0x01
#define NI_NUMERICHOST      0x02
#define NI_NAMEREQD         0x04
#define NI_NUMERICSERV      0x08
#define NI_NUMERICSCOPE     0x10
#define NI_DGRAM            0x20

#define NI_MAXHOST          1025
#define NI_MAXSERV          32

/**** TYPES ****/

struct addrinfo {
    int             ai_flags;       // Input flags
    int             ai_family;      // Address family
    int             ai_socktype;    // Socket type
    int             ai_protocol;    // Protocol of socket
    socklen_t       ai_addrlen;     // Address length
    struct sockaddr *ai_addr;       // Address
    char            *ai_canonname;  // Canonical name
    struct addrinfo *ai_next;       // Pointer to next in list
};

struct hostent {
    char *h_name;                   // Official name of the host
    char **h_aliases;               // Alternative host names
    int h_addrtype;                 // Address type
    int h_length;                   // The length, in bytes, of the address
    char **h_addr_list;             // Alternative host addresses
};

struct netent {
    char *n_name;                   // FQDN of the host
    char **n_aliases;               // Alternative network names
    int n_addrtype;                 // The address type of the network
    uint32_t n_net;                 // The network number
};

struct protoent {
    char *p_name;                   // Official name of the protocol
    char **p_aliases;               // Alternative protocol names
    int p_proto;                    // Protocol number
};

struct servent {
    char *s_name;                   // Service name
    char **s_aliases;               // Service aliases
    int s_port;
    char *s_proto;
};

/**** VARIABLES ****/

extern int h_errno;

/**** FUNCTIONS ****/

void             endhostent(void);
void             endnetent(void);
void             endprotoent(void);
void             endservent(void);
struct hostent  *gethostbyaddr(const void *addr, size_t len, int type);
struct hostent  *gethostbyname(const char *name);
struct hostent  *gethostent(void);
struct netent   *getnetbyaddr(uint32_t net, int type);
struct netent   *getnetbyname(const char *name);
struct netent   *getnetent(void);
struct protoent *getprotobyname(const char *name);
struct protoent *getprotobynumber(int proto);
struct protoent *getprotoent(void);
struct servent  *getservbyname(const char *name, const char *proto);
struct servent  *getservbyport(int port, const char *proto);
struct servent  *getservent(void);
void             sethostent(int stayopen);
void             setnetent(int stayopen);
void             setprotoent(int stayopen);
void             setservent(int stayopen);
void             herror(const char *s);
const char      *hstrerror(int err);


#endif


_End_C_Header