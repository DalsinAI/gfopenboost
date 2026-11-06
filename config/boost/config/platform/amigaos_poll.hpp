//  GFOpenBoost: poll() names for Boost.Asio. Asio on AmigaOS waits with
//  select() only (the Symbian path, see patches/headers), so this just gives
//  the types and flags its headers name. Included in place of <sys/poll.h>.
//
//  Copyright (c) 2026 AmiCradle Team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CONFIG_PLATFORM_AMIGAOS_POLL_HPP
#define BOOST_CONFIG_PLATFORM_AMIGAOS_POLL_HPP
#if defined(__has_include)
#  if __has_include(<sys/poll.h>)
#    include <sys/poll.h>
#    define BOOST_AMIGAOS_HAVE_POLL_H
#  elif __has_include(<poll.h>)
#    include <poll.h>
#    define BOOST_AMIGAOS_HAVE_POLL_H
#  endif
#endif
#ifndef BOOST_AMIGAOS_HAVE_POLL_H
struct pollfd {
    int fd;
    short events;
    short revents;
};
#  define POLLIN     0x0001
#  define POLLPRI    0x0002
#  define POLLOUT    0x0004
#  define POLLERR    0x0008
#  define POLLHUP    0x0010
#  define POLLNVAL   0x0020
#endif
#endif
