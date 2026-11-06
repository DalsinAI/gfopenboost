// GFOpenBoost test: one program that uses every compiled part, run in a lab.
// Prints one line a check and "boosttest: N failed" at the end; the return
// code is 0 only when all passed.
// Copyright (c) 2026 AmiCradle Team
// Distributed under the Boost Software License, Version 1.0.
#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <boost/regex.hpp>
#include <boost/cregex.hpp>
#include <boost/program_options.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>
#include <boost/locale.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/timer/timer.hpp>
#include <boost/chrono.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/container/pmr/monotonic_buffer_resource.hpp>
#include <boost/container/pmr/vector.hpp>
#include <boost/charconv.hpp>
#include <boost/atomic.hpp>
#include <boost/asio.hpp>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

static int failed = 0;
static void check(const char *what, bool ok, const std::string &detail = std::string())
{
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str());
    std::fflush(stdout);
    if (!ok)
        failed++;
}

template <class F> static void guarded(const char *what, F f)
{
    try {
        f();
    } catch (const std::exception &e) {
        check(what, false, std::string("exception ") + e.what());
    } catch (...) {
        check(what, false, "unknown exception");
    }
}

int main(int argc, char **argv)
{
    std::printf("boosttest: Boost %s\n", BOOST_LIB_VERSION);

    guarded("filesystem", [] {
        namespace fs = boost::filesystem;
        fs::path dir = fs::path("T:") / "gfopenboost-test";
        boost::system::error_code ec;
        fs::create_directories(dir, ec);
        fs::path f = dir / "a.txt";
        { std::FILE *o = std::fopen(f.string().c_str(), "w"); if (o) { std::fputs("hello", o); std::fclose(o); } }
        check("filesystem exists/size", fs::exists(f) && fs::file_size(f) == 5, f.string());
        int n = 0;
        for (fs::directory_iterator it(dir), end; it != end; ++it) n++;
        check("filesystem directory_iterator", n == 1, std::to_string(n));
        fs::space_info si = fs::space(dir, ec);
        check("filesystem space (dos.library Info)", !ec && si.capacity > 0, std::to_string((unsigned long long)si.capacity));
        fs::remove_all(dir, ec);
        check("filesystem remove_all", !fs::exists(dir));
    });

    guarded("thread", [] {
        boost::atomic<int> count(0);
        boost::thread_group g;
        for (int i = 0; i < 4; i++) g.create_thread([&] { for (int j = 0; j < 1000; j++) count++; });
        g.join_all();
        check("thread group + atomic", count == 4000, std::to_string(count.load()));
        auto fu = boost::async([] { return 42; });
        check("thread future", fu.get() == 42);
        boost::mutex m; boost::condition_variable cv; bool ready = false;
        boost::thread t([&] { boost::lock_guard<boost::mutex> l(m); ready = true; cv.notify_one(); });
        { boost::unique_lock<boost::mutex> l(m);
          bool ok = cv.wait_for(l, boost::chrono::seconds(5), [&] { return ready; });
          check("thread condition_variable wait_for", ok); }
        t.join();
    });

    guarded("regex", [] {
        boost::regex r("(\\w+)@(\\w+)\\.org");
        boost::smatch m;
        std::string s = "mail amiga@example.org now";
        check("regex search", boost::regex_search(s, m, r) && m[2] == "example");
        std::string big(20000, 'a');
        check("regex long input", boost::regex_match(big, boost::regex("a*")));
        boost::regex_tA re; check("regex posix api", boost::regcompA(&re, "^ab+c$", boost::REG_EXTENDED) == 0
            && boost::regexecA(&re, "abbbc", 0, 0, 0) == 0); boost::regfreeA(&re);
    });

    guarded("program_options", [&] {
        namespace po = boost::program_options;
        po::options_description d("o");
        d.add_options()("size", po::value<int>()->default_value(3), "size");
        const char *av[] = {"x", "--size=7"};
        po::variables_map vm;
        po::store(po::parse_command_line(2, av, d), vm);
        po::notify(vm);
        check("program_options", vm["size"].as<int>() == 7);
        (void)argc; (void)argv;
    });

    guarded("json", [] {
        boost::json::value v = boost::json::parse("{\"a\":[1,2,3],\"b\":\"x\"}");
        check("json parse/serialize", boost::json::serialize(v) == "{\"a\":[1,2,3],\"b\":\"x\"}");
    });

    guarded("url", [] {
        boost::urls::url u = boost::urls::parse_uri("https://amiga.example/path/x?q=1#f").value();
        check("url", u.host() == "amiga.example" && u.query() == "q=1");
    });

    guarded("locale", [] {
        boost::locale::generator gen;
        std::locale loc = gen("en_GB.UTF-8");
        std::string up = boost::locale::to_upper("amiga", loc);
        check("locale to_upper", up == "AMIGA", up);
        std::wstring w = boost::locale::conv::utf_to_utf<wchar_t>(std::string("\xc3\xa9t\xc3\xa9"));
        check("locale utf8->wide", w.size() == 3);
        std::string l1 = boost::locale::conv::from_utf(std::string("caf\xc3\xa9"), "ISO-8859-1");
        check("locale to Latin-1", l1 == std::string("caf\xe9"));
    });

    guarded("iostreams", [] {
        namespace io = boost::iostreams;
        io::stream<io::file_descriptor_sink> o("T:gfopenboost-io.txt");
        o << "io-test"; o.close();
        io::stream<io::file_descriptor_source> i("T:gfopenboost-io.txt");
        std::string s; i >> s;
        check("iostreams file_descriptor", s == "io-test", s);
        std::remove("T:gfopenboost-io.txt");
    });

    guarded("serialization", [] {
        std::vector<int> a{1, 2, 3}, b;
        std::stringstream ss;
        { boost::archive::text_oarchive oa(ss); oa << a; }
        { boost::archive::text_iarchive ia(ss); ia >> b; }
        check("serialization text archive", a == b);
    });

    guarded("date_time", [] {
        boost::gregorian::date d(2026, boost::gregorian::Nov, 6);
        check("date_time", d.day_of_week() == boost::date_time::Friday && boost::gregorian::to_simple_string(d) == "2026-Nov-06",
              boost::gregorian::to_simple_string(d));
    });

    guarded("chrono+timer", [] {
        boost::timer::cpu_timer t;
        boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        boost::timer::cpu_times e = t.elapsed();
        check("timer wall >= 90 ms", e.wall >= 90000000LL, std::to_string((long long)(e.wall / 1000000)) + " ms");
    });

    guarded("container pmr", [] {
        char buf[512];
        boost::container::pmr::monotonic_buffer_resource r(buf, sizeof buf);
        boost::container::pmr::vector<int> v(&r);
        for (int i = 0; i < 10; i++) v.push_back(i);
        check("container pmr", v.size() == 10 && v[9] == 9);
    });

    guarded("charconv", [] {
        double d = 0; const char s[] = "3.25";
        auto r = boost::charconv::from_chars(s, s + 4, d);
        char out[32]; auto w = boost::charconv::to_chars(out, out + sizeof out, 1.5);
        check("charconv", r.ec == std::errc() && d == 3.25 && std::string(out, w.ptr) == "1.5");
    });

    guarded("asio", [] {
        boost::asio::io_context io;
        boost::asio::steady_timer t(io, std::chrono::milliseconds(50));
        bool fired = false;
        t.async_wait([&](const boost::system::error_code &) { fired = true; });
        io.run();
        check("asio steady_timer", fired);
        boost::system::error_code ec;
        auto a = boost::asio::ip::make_address("192.168.0.1", ec);
        check("asio address", !ec && a.to_string() == "192.168.0.1");
    });

    std::printf("boosttest: %d failed\n", failed);
    return failed ? 20 : 0;
}
