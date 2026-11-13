// GFOpenBoost test: UDP as libtorrent-rasterbar 2.0 uses it (GFOpenTorrent).
// boosttest's "asio-amiga udp" covers one async_receive_from on loopback;
// libtorrent's udp_socket instead binds 0.0.0.0, goes non-blocking, waits
// with async_wait(wait_read) and drains with receive_from() until
// would_block; those are checks. Local service discovery's multicast group
// and SIOCGIFCONF are reported as INFO lines: GFOpenSocket refuses both by
// design, and GFOpenTorrent does without them. One line a check, as boosttest,
// and "udplt: N failed" at the end; the return code is 0 only when all
// passed. Builds on the host too (g++ -std=c++17 tests/udplt.cpp -pthread)
// so the checks themselves are tested where a stack is known good.
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
#include <boost/asio.hpp>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <unistd.h>
#include <chrono>
#if defined(__amigaos__) || defined(__AMIGA__)
#include <sys/sockio.h>
#endif

namespace asio = boost::asio;
using asio::ip::udp;
using boost::system::error_code;

static int failed = 0;
static void check(const char *what, bool ok, const std::string &detail = std::string())
{
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str());
    std::fflush(stdout);
    if (!ok)
        failed++;
}
static std::string msg(const error_code &ec) { return ec ? ec.message() : std::string(); }
// a short sleep through Asio's own timer (the reactor's wait), not usleep()
static void sleep_ms(int ms)
{
    asio::io_context io;
    asio::steady_timer t(io, std::chrono::milliseconds(ms));
    t.wait();
}
static void info(const char *what, const std::string &detail)
{
    std::printf("INFO %s: %s\n", what, detail.c_str());
    std::fflush(stdout);
}

// libtorrent's udp_socket::bind(): reuse_address, bind, non_blocking(true)
static bool lt_bind(udp::socket &s, const udp::endpoint &ep, std::string &why)
{
    error_code ec;
    s.open(ep.protocol(), ec);
    if (!ec) s.set_option(udp::socket::reuse_address(true), ec);
    if (!ec) s.bind(ep, ec);
    if (!ec) s.non_blocking(true, ec);
    if (ec) why = ec.message();
    return !ec;
}

// libtorrent's udp_socket::read(): receive_from until would_block
static int lt_drain(udp::socket &s, std::string &got, error_code &last)
{
    char buf[1500];
    int n = 0;
    for (;;) {
        udp::endpoint from;
        std::size_t len = s.receive_from(asio::buffer(buf), from, 0, last);
        if (last == asio::error::would_block || last == asio::error::try_again)
            return n;
        if (last)
            return n;
        got.append(buf, len);
        n++;
    }
}

int main()
{
    // 1. bind as libtorrent does, and the buffer sizes the session sets
    {
        asio::io_context io;
        udp::socket a(io);
        std::string why;
        bool ok = lt_bind(a, udp::endpoint(asio::ip::address_v4::any(), 0), why);
        check("bind 0.0.0.0:0 reuse_address non_blocking", ok && a.local_endpoint().port() != 0, why);
        error_code ec;
        a.set_option(asio::socket_base::receive_buffer_size(256 * 1024), ec);
        check("receive_buffer_size", !ec, msg(ec));
        a.set_option(asio::socket_base::send_buffer_size(256 * 1024), ec);
        check("send_buffer_size", !ec, msg(ec));
    }

    // 2. async_wait(wait_read), drain to would_block, re-arm, and send from
    //    the bound socket (DHT and uTP answer from the socket they listen on)
    {
        asio::io_context io;
        udp::socket a(io), b(io);
        std::string why;
        bool ok = lt_bind(a, udp::endpoint(asio::ip::address_v4::any(), 0), why)
               && lt_bind(b, udp::endpoint(asio::ip::address_v4::any(), 0), why);
        check("two sockets bound", ok, why);
        if (ok) {
            udp::endpoint to_a(asio::ip::address_v4::loopback(), a.local_endpoint().port());
            udp::endpoint to_b(asio::ip::address_v4::loopback(), b.local_endpoint().port());
            int rounds = 0, packets = 0;
            std::string got, err;
            bool timeout = false;
            asio::steady_timer guard(io, std::chrono::seconds(5));
            guard.async_wait([&](const error_code &e) { if (!e) { timeout = true; a.cancel(); } });
            std::function<void(const error_code &)> on_read = [&](const error_code &e) {
                if (e) { err = e.message(); return; }
                error_code last;
                packets += lt_drain(a, got, last);
                if (last != asio::error::would_block && last != asio::error::try_again)
                    err = "drain: " + last.message();
                if (++rounds == 1) {
                    error_code se;
                    b.send_to(asio::buffer("four", 4), to_a, 0, se);
                    a.async_wait(udp::socket::wait_read, on_read);
                } else {
                    guard.cancel();
                }
            };
            a.async_wait(udp::socket::wait_read, on_read);
            error_code se;
            b.send_to(asio::buffer("one", 3), to_a, 0, se);
            b.send_to(asio::buffer("two", 3), to_a, 0, se);
            b.send_to(asio::buffer("three", 5), to_a, 0, se);
            check("send_to non-blocking x3", !se, msg(se));
            io.run();
            check("async_wait(wait_read) then drain to would_block, twice",
                  !timeout && rounds == 2 && packets == 4 && got == "onetwothreefour",
                  "rounds=" + std::to_string(rounds) + " packets=" + std::to_string(packets) + " got=" + got
                  + (timeout ? " timeout" : "") + (err.empty() ? "" : " " + err));

            // a uTP-sized datagram back the other way
            std::string big(1400, 'u');
            a.send_to(asio::buffer(big), to_b, 0, se);
            check("send_to 1400 bytes", !se, msg(se));
            char buf[1500];
            std::size_t n = 0;
            error_code re;
            for (int i = 0; i < 50; i++) {
                udp::endpoint from;
                n = b.receive_from(asio::buffer(buf), from, 0, re);
                if (re != asio::error::would_block && re != asio::error::try_again)
                    break;
                sleep_ms(20);
            }
            check("receive_from 1400 bytes", !re && n == 1400 && !std::memcmp(buf, big.data(), n),
                  msg(re) + " n=" + std::to_string(n));
        }
    }

    // 3. local service discovery: 239.192.152.143:6771, as lsd.cpp sets it up
    {
        asio::io_context io;
        udp::socket s(io);
        std::string why;
        bool ok = lt_bind(s, udp::endpoint(asio::ip::address_v4::any(), 6771), why);
        check("LSD bind 0.0.0.0:6771", ok, why);
        if (ok) {
            const auto group = asio::ip::make_address_v4("239.192.152.143");
            error_code ec;
            s.set_option(asio::ip::multicast::join_group(group), ec);
            // GFOpenSocket opens only mDNS's group to the guest (policy of
            // 3 Oct 2026), so LSD's join and send are refused there; libtorrent
            // carries on without LSD. Reported, not failed.
            info("LSD join_group 239.192.152.143", ec ? ec.message() : "ok");
            s.set_option(asio::ip::multicast::enable_loopback(true), ec);
            check("LSD enable_loopback", !ec, msg(ec));
            s.set_option(asio::ip::multicast::hops(32), ec);
            check("LSD hops 32", !ec, msg(ec));
            s.send_to(asio::buffer("BT-SEARCH", 9), udp::endpoint(group, 6771), 0, ec);
            info("LSD send_to group", ec ? ec.message() : "ok");
            char buf[64];
            std::size_t n = 0;
            error_code re;
            for (int i = 0; i < 50; i++) {
                udp::endpoint from;
                n = s.receive_from(asio::buffer(buf), from, 0, re);
                if (re != asio::error::would_block && re != asio::error::try_again)
                    break;
                sleep_ms(20);
            }
            // looped-back multicast needs a multicast route; report, don't fail
            info("LSD own announce looped back", !re && n == 9 ? "yes" : "no (" + re.message() + ")");
        }
    }

    // 4. interfaces, as enum_net.cpp's TORRENT_USE_IFCONF path lists them
    {
#ifdef SIOCGIFCONF
        int s = ::socket(AF_INET, SOCK_DGRAM, 0);
        struct ifreq buf[40];
        struct ifconf ifc;
        std::memset(buf, 0, sizeof buf);
        ifc.ifc_len = sizeof buf;
        ifc.ifc_req = buf;
        int r = s >= 0 ? ::ioctl(s, SIOCGIFCONF, &ifc) : -1;
        int err = errno;
        std::string names;
        int v4 = 0;
        if (r == 0) {
            const char *p = reinterpret_cast<const char *>(ifc.ifc_buf);
            for (int left = ifc.ifc_len; left > 0;) {
                const struct ifreq &it = *reinterpret_cast<const struct ifreq *>(p);
#if defined(_SIZEOF_ADDR_IFREQ)
                int sz = _SIZEOF_ADDR_IFREQ(it);
#elif defined(__amigaos__) || defined(__AMIGA__)
                int sz = it.ifr_addr.sa_len > sizeof(struct sockaddr) ? IFNAMSIZ + it.ifr_addr.sa_len : (int)sizeof(struct ifreq);
#else
                int sz = (int)sizeof(struct ifreq);
#endif
                if (left < sz) break;
                if (it.ifr_addr.sa_family == AF_INET) {
                    v4++;
                    names += std::string(" ") + std::string(it.ifr_name, strnlen(it.ifr_name, IFNAMSIZ));
                }
                p += sz;
                left -= sz;
            }
        }
        if (s >= 0) ::close(s);
        // GFOpenSocket answers EOPNOTSUPP; GFOpenTorrent lists interfaces
        // with Roadshow's ObtainInterfaceList()/QueryInterfaceTagList()
        // instead. Reported, not failed.
        info("SIOCGIFCONF", r == 0 ? std::to_string(v4) + " IPv4:" + names : std::string("errno ") + std::strerror(err));
#else
        info("SIOCGIFCONF", "not in the headers");
#endif
    }

    // 5. the default route's local address: connect() a UDP socket (no
    //    packet goes out) and read local_endpoint(); a fallback for
    //    enum_routes() where the stack has no route table call
    {
        asio::io_context io;
        udp::socket s(io, udp::v4());
        error_code ec;
        s.connect(udp::endpoint(asio::ip::make_address_v4("192.0.2.1"), 9), ec);
        auto local = ec ? udp::endpoint() : s.local_endpoint(ec);
        info("default route via UDP connect()", ec ? ec.message() : local.address().to_string());
    }

    std::printf("udplt: %d failed\n", failed);
    return failed ? 20 : 0;
}
