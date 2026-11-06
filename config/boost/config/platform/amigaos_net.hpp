//  GFOpenBoost: the POSIX network names Boost.Asio uses that libnix and the
//  NDK's network headers lack. Included by boost/asio/detail/socket_types.hpp
//  on AmigaOS only. The functions are in libboost_asio.a
//  (compat/src/amiga_net.c). IPv4 only: the stack has no IPv6, so a v6
//  socket fails when it is made.
//
//  From aMule's port (amigaamule_posix.h), now shared.
//  Copyright (c) 2026 AmiCradle Team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONFIG_PLATFORM_AMIGAOS_NET_HPP
#define BOOST_CONFIG_PLATFORM_AMIGAOS_NET_HPP
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <netdb.h>

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef IF_NAMESIZE
#define IF_NAMESIZE 16
#endif
#ifndef AF_INET6
#define AF_INET6 28
#endif
#ifndef PF_INET6
#define PF_INET6 AF_INET6
#endif
#ifndef IPPROTO_IPV6
#define IPPROTO_IPV6 41
#endif
#ifndef IPPROTO_ICMPV6
#define IPPROTO_ICMPV6 58
#endif
#ifndef IPV6_UNICAST_HOPS
#define IPV6_UNICAST_HOPS 4
#endif
#ifndef IPV6_MULTICAST_IF
#define IPV6_MULTICAST_IF 9
#endif
#ifndef IPV6_MULTICAST_HOPS
#define IPV6_MULTICAST_HOPS 10
#endif
#ifndef IPV6_MULTICAST_LOOP
#define IPV6_MULTICAST_LOOP 11
#endif
#ifndef IPV6_JOIN_GROUP
#define IPV6_JOIN_GROUP 12
#endif
#ifndef IPV6_LEAVE_GROUP
#define IPV6_LEAVE_GROUP 13
#endif
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif

// Ports that declared these themselves define GFOPENBOOST_HAVE_SOCKADDR_STORAGE.
#ifndef GFOPENBOOST_HAVE_SOCKADDR_STORAGE
#define GFOPENBOOST_HAVE_SOCKADDR_STORAGE
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    char ss_pad[126];
} __attribute__((aligned(4)));
struct ipv6_mreq {
    struct in6_addr ipv6mr_multiaddr;
    unsigned int ipv6mr_interface;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);
unsigned int if_nametoindex(const char *name);
char *if_indextoname(unsigned int index, char *name);
ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);
// libnix's socket library has these; its headers do not declare them
int socketpair(int domain, int type, int protocol, int sv[2]);
int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
int getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
                size_t hostlen, char *serv, size_t servlen, int flags);
// declared for Asio's gethostbyname wrappers, unused while getaddrinfo() is there
int gethostbyname_r(const char *name, struct hostent *ret, char *buf,
                    size_t buflen, struct hostent **result, int *h_errnop);
int gethostbyaddr_r(const void *addr, socklen_t len, int type,
                    struct hostent *ret, char *buf, size_t buflen,
                    struct hostent **result, int *h_errnop);
#ifdef __cplusplus
}
#endif

// exec/lists.h's macro would rewrite members named IsListEmpty
#undef IsListEmpty
#endif
