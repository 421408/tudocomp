#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>
#include <glog/logging.h>

#include <tudocomp/io.hpp>
#include <tudocomp/util.hpp>
#include <tudocomp/util/View.hpp>
#include <tudocomp/util/GenericView.hpp>
#include <tudocomp/Compressor.hpp>
#include <tudocomp/Algorithm.hpp>

#include <tudocomp/io/MMapHandle.hpp>

#include "test/util.hpp"

// Test that DEBUG and NDEBUG are defined correctly
#if defined(DEBUG) && defined(NDEBUG)
#error Both DEBUG and NDEBUG are defined!
#endif

#if !defined(DEBUG) && !defined(NDEBUG)
#error Neither DEBUG nor NDEBUG are defined!
#endif

using namespace tdc;
using namespace tdc::io;

TEST(Test, test_file) {
    ASSERT_EQ(test::test_file_path("test.txt"), "test_files/test.txt");
    test::write_test_file("test.txt", "foobar");
    ASSERT_EQ(test::read_test_file("test.txt"), "foobar");

    std::string err;
    try {
        test::read_test_file("not_test.txt");
    } catch (std::runtime_error e) {
        err = e.what();
    }
    ASSERT_EQ("Could not open test file \"test_files/not_test.txt\"", err);
}

TEST(Test, test_file_null) {
    test::write_test_file("test_nte.txt", "foo\0bar\0"_v);
    ASSERT_EQ(test::read_test_file("test_nte.txt"), "foo\0bar\0"_v);
    ASSERT_NE(test::read_test_file("test_nte.txt"), "foo");
}

TEST(View, string_conv_null) {
    ASSERT_EQ("abc\0def\0"_v, std::string("abc\0def\0"_v));
    ASSERT_NE("abc\0dez\0"_v, std::string("abc\0def\0"_v));
    ASSERT_NE("abc\0def\0"_v, std::string("abc\0dez\0"_v));

    std::string x("x");
    x = x + std::string("abc\0def\0"_v);

    ASSERT_EQ(x, "xabc\0def\0"_v);
    ASSERT_NE(x, "xabc\0dez\0"_v);

    ASSERT_EQ(std::string("\xff\0"_v).size(), 2U);
}

TEST(Util, bits_for) {
    ASSERT_EQ(bits_for(0b0), 1u);
    ASSERT_EQ(bits_for(0b1), 1u);
    ASSERT_EQ(bits_for(0b10), 2u);
    ASSERT_EQ(bits_for(0b11), 2u);
    ASSERT_EQ(bits_for(0b100), 3u);
    ASSERT_EQ(bits_for(0b111), 3u);
    ASSERT_EQ(bits_for(0b1000), 4u);
    ASSERT_EQ(bits_for(0b1111), 4u);
    ASSERT_EQ(bits_for(0b10000), 5u);
    ASSERT_EQ(bits_for(0b11111), 5u);
    ASSERT_EQ(bits_for(0b100000), 6u);
    ASSERT_EQ(bits_for(0b111111), 6u);
    ASSERT_EQ(bits_for(0b1000000), 7u);
    ASSERT_EQ(bits_for(0b1111111), 7u);
    ASSERT_EQ(bits_for(0b10000000), 8u);
    ASSERT_EQ(bits_for(0b11111111), 8u);
    ASSERT_EQ(bits_for(0b100000000), 9u);
    ASSERT_EQ(bits_for(0b111111111), 9u);
}

TEST(Util, bytes_for) {
    ASSERT_EQ(bytes_for(0x00), 1u);

    size_t l = 0x01U;
    size_t h = 0xFFU;

    for(size_t i = 0; i < sizeof(size_t) - 1; i++) {
        ASSERT_EQ(i+1, bytes_for(l));
        ASSERT_EQ(i+1, bytes_for(h));
        l <<= 8U; h <<= 8U;
    }
}

TEST(Util, pack_integers) {
    ASSERT_EQ(test::pack_integers({
        0b1111, 4,
        0b1001, 4,
    }), (std::vector<uint8_t> {
        0b11111001
    }));
    ASSERT_EQ(test::pack_integers({
        0b1111, 2,
    }), (std::vector<uint8_t> {
        0b11000000
    }));
    ASSERT_EQ(test::pack_integers({
        0b111, 3,
        0b1101, 4,
        0b11001, 5,
        0b110001, 6,
    }), (std::vector<uint8_t> {
        0b11111011,
        0b10011100,
        0b01000000,
    }));
    ASSERT_EQ(test::pack_integers({
        0b001000, 6,
    }), (std::vector<uint8_t> {
        0b00100000,
    }));
    ASSERT_EQ(test::pack_integers({
        0b001000, 6,
        0b001100001, 9,
        0b001100010, 9
    }), (std::vector<uint8_t> {
        0b00100000,
        0b11000010,
        0b01100010,
    }));
    ASSERT_EQ(test::pack_integers({
        9, 64,
    }), (std::vector<uint8_t> {
        0,0,0,0,0,0,0,9
    }));
}

TEST(Input, vector) {
    std::vector<uint8_t> v { 97, 98, 99 };

    {
        Input inp = Input::from_memory(v);
        auto ref = inp.as_view();

        ASSERT_EQ(ref, "abc");
    }

    {
        Input inp = Input::from_memory(v);
        auto stream = inp.as_stream();

        std::string s;

        stream >> s;

        ASSERT_EQ(s, "abc");
    }
}

TEST(Input, string_ref) {
    string_ref v { "abc" };

    {
        Input inp = Input::from_memory(v);
        auto ref = inp.as_view();

        ASSERT_EQ(ref, "abc");
    }

    {
        Input inp = Input::from_memory(v);
        auto stream = inp.as_stream();

        std::string s;

        stream >> s;

        ASSERT_EQ(s, "abc");
    }
}

std::string fn(std::string suffix) {
    return "tudocomp_tests_" + suffix;
}

TEST(Input, file) {
    test::write_test_file(fn("short.txt"), "abc");

    {
        Input inp = Input::from_path(test::test_file_path(fn("short.txt")));
        auto ref = inp.as_view();

        ASSERT_EQ(ref, "abc");
    }

    {
        Input inp = Input::from_path(test::test_file_path(fn("short.txt")));
        auto stream = inp.as_stream();

        std::string s;

        stream >> s;

        ASSERT_EQ(s, "abc");
    }
}

void stream_moving(Input& i) {
    {
        InputStream s1 = i.as_stream();
        ASSERT_EQ(s1.get(), 'a');
        InputStream s2(std::move(s1));
        ASSERT_EQ(s2.get(), 'b');
        InputStream s3(std::move(s2));
        ASSERT_EQ(s3.get(), 'c');
    }
    {
        InputStream s4 = i.as_stream();
        ASSERT_EQ(s4.get(), 'a');
        InputStream s5(std::move(s4));
        ASSERT_EQ(s5.get(), 'b');
    }
}

TEST(Input, stream_moving_vec) {
    std::vector<uint8_t> v { 97, 98, 99, 100, 101 };
    Input i(v);
    stream_moving(i);
}

TEST(Input, stream_moving_mem) {
    string_ref v { "abcde" };
    Input i(v);
    stream_moving(i);
}

TEST(Input, stream_moving_file) {
    test::write_test_file(fn("stream_moving.txt"), "abcde");
    Input i = Input::from_path(test::test_file_path(fn("stream_moving.txt")));
    stream_moving(i);
}

TEST(Input, stream_iterator) {
    Input i("asdf");

    std::stringstream ss;

    for (uint8_t x : i.as_stream()) {
        ss << x;
    }

    ASSERT_EQ(ss.str(), "asdf");
}

TEST(Input, ensure_null_term) {
    std::vector<uint8_t> a  { 96, 97, 98, 99, 100, 101, 102 };
    std::vector<uint8_t> a2 { 96, 97, 98, 99, 100, 101, 102 };
    std::vector<uint8_t> b  { 96, 97, 98 };
    std::vector<uint8_t> c  { 96, 97, 98, 0 };

    {
        Input i(View(a).slice(0, 3));
        auto x = i.as_view();
        std::vector<uint8_t> y = x;
        ASSERT_EQ(y, b);
        ASSERT_NE(y, c);
    }
    ASSERT_EQ(a, a2);

    {
        Input i(View(a).slice(0, 3));
        i = Input(i, InputRestrictions({0}, true));
        auto x = i.as_view();
        ASSERT_NE(x, b);
        ASSERT_EQ(x, c);
    }
    ASSERT_EQ(a, a2);

    {
        Input i(View(a).slice(0, 3));
        Input i2 = std::move(i);
        i2 = Input(i2, InputRestrictions({0}, true));
        auto x = i2.as_view();
        ASSERT_NE(x, b);
        ASSERT_EQ(x, c);
    }
    ASSERT_EQ(a, a2);

    {
        Input i(View(a).slice(0, 3));
        i = Input(i, InputRestrictions({0}, true));

        Input i2 = i;

        auto x = i2.as_view();
        ASSERT_NE(x, b);
        ASSERT_EQ(x, c);

        auto y = i.as_view();
        ASSERT_NE(y, b);
        ASSERT_EQ(y, c);
    }
    ASSERT_EQ(a, a2);

    {
        Input i(View(a).slice(0, 3));

        i = Input(i, InputRestrictions({0}, true));
        {
            auto x = i.as_view();
            ASSERT_NE(x, b);
            ASSERT_EQ(x, c);
        }
        {
            auto x = i.as_view();
            ASSERT_NE(x, b);
            ASSERT_EQ(x, c);
        }
    }
    ASSERT_EQ(a, a2);
}

namespace input_nte_matrix {
    using Path = io::Path;

    std::vector<uint8_t>       slice_buf       { 97,  98,  99, 100, 101, 102 };
    const View                 slice           = View(slice_buf).slice(0, 3);
    const std::vector<uint8_t> o_slice         { 97,  98,  99  };

    template<class T, class CStrat>
    void matrix_row(T input,
                    View expected_output,
                    CStrat i_copy_strat,
                    std::function<void(Input&, View)> i_out_compare) {

        i_copy_strat(std::move(input),
                     expected_output,
                     [](Input&) {},
                     i_out_compare);
    }

    View view(View i) {
        return i;
    }

    io::Path file(View i) {
        std::hash<std::string> hasher;

        std::string basename = std::string("matrix_test_file_path") + std::string(i);
        //std::cout << "basename before: " << basename;
        std::stringstream ss;
        ss << hasher(basename);
        basename = ss.str() + ".txt";
        //std::cout << " basename after: " << basename << "\n";

        test::write_test_file(basename, i);
        return io::Path { test::test_file_path(basename) };
    }

    template<class T>
    void no_copy(T i,
                 View o,
                 std::function<void(Input&)> i_nte_mod,
                 std::function<void(Input&, View)> i_out_compare) {
        Input target_input(std::move(i));
        i_nte_mod(target_input);

        i_out_compare(target_input, o);
    }

    template<class T>
    void copy(T i,
              View o,
              std::function<void(Input&)> i_nte_mod,
              std::function<void(Input&, View)> i_out_compare) {
        Input target_input;
        {
            Input source_input(std::move(i));
            i_nte_mod(source_input);
            target_input = source_input;
            i_out_compare(source_input, o);
        }

        i_out_compare(target_input, o);
    }

    template<class T>
    void move(T i,
              View o,
              std::function<void(Input&)> i_nte_mod,
              std::function<void(Input&, View)> i_out_compare) {
        Input target_input;
        {
            Input source_input(std::move(i));
            i_nte_mod(source_input);
            target_input = std::move(source_input);
        }

        i_out_compare(target_input, o);
    }

    void out_view(Input& i_, View o) {
        std::vector<uint8_t> b = o;
        std::vector<uint8_t> c = o;
        if (c.size() == 0 || c.back() != 0) {
            c.push_back(0);
        }

        // First, do a bunch of tests on the copies to check for the null terminator
        // behaving correctly.

        Input i_bak = i_;

        {
            Input i = i_bak;
            std::vector<uint8_t> y = i.as_view();
            ASSERT_EQ(y, b);
            ASSERT_NE(y, c);
        }
        {
            Input i = i_bak;
            i = Input(i, InputRestrictions({0}, true));
            auto x = i.as_view();
            ASSERT_NE(x, b);
            ASSERT_EQ(x, c);
        }

        {
            Input i = i_bak;
            i = Input(i, InputRestrictions({0}, true));
            Input i2 = i;

            auto x = i2.as_view();
            ASSERT_NE(x, b);
            ASSERT_EQ(x, c);

            auto y = i.as_view();
            ASSERT_NE(y, b);
            ASSERT_EQ(y, c);
        }

        {
            Input i = i_bak;
            i = Input(i, InputRestrictions({0}, true));
            {
                auto x = i.as_view();
                ASSERT_NE(x, b);
                ASSERT_EQ(x, c);
            }
            {
                auto x = i.as_view();
                ASSERT_NE(x, b);
                ASSERT_EQ(x, c);
            }
        }

        // Then ensure we actually skip i forward in the end:
        i_.as_view();
    }

    void out_stream(Input& i, View o) {
        auto tmp = i.as_stream();
        std::stringstream ss;
        ss << tmp.rdbuf();
        std::string a_ = ss.str();
        std::vector<uint8_t> a = View(a_);
        std::vector<uint8_t> b = o;
        ASSERT_EQ(a, b);
    }

    TEST(InputNteMatrix, n1) {
        matrix_row(view(slice), View(o_slice), no_copy<View>, out_view);
    }

    TEST(InputNteMatrix, n2) {
        matrix_row(view(slice), View(o_slice), no_copy<View>, out_stream);
    }

    TEST(InputNteMatrix, n7) {
        matrix_row(view(slice), View(o_slice), copy<View>, out_view);
    }

    TEST(InputNteMatrix, n8) {
        matrix_row(view(slice), View(o_slice), copy<View>, out_stream);
    }

    TEST(InputNteMatrix, n13) {
        matrix_row(view(slice), View(o_slice), move<View>, out_view);
    }

    TEST(InputNteMatrix, n14) {
        matrix_row(view(slice), View(o_slice), move<View>, out_stream);
    }

    TEST(InputNteMatrix, n19) {
        matrix_row(file(slice), View(o_slice), no_copy<Path>, out_view);
    }

    TEST(InputNteMatrix, n20) {
        matrix_row(file(slice), View(o_slice), no_copy<Path>, out_stream);
    }

    TEST(InputNteMatrix, n25) {
        matrix_row(file(slice), View(o_slice), copy<Path>, out_view);
    }

    TEST(InputNteMatrix, n26) {
        matrix_row(file(slice), View(o_slice), copy<Path>, out_stream);
    }

    TEST(InputNteMatrix, n31) {
        matrix_row(file(slice), View(o_slice), move<Path>, out_view);
    }

    TEST(InputNteMatrix, n32) {
        matrix_row(file(slice), View(o_slice), move<Path>, out_stream);
    }

}

TEST(Input, escaping_view) {
    Input i("\0\x01\xff\xfe\0"_v);
    i = Input(i, InputRestrictions({0}, true));
    auto v = i.as_view();
    ASSERT_EQ(View(v), "\xff\xfe\x01\xff\xff\xfe\xff\xfe\0"_v);
}

TEST(Input, escaping_stream) {
    Input i("\0\x01\xff\xfe\0"_v);
    i = Input(i, InputRestrictions({0}, true));
    auto s = i.as_stream();
    std::stringstream ss;
    ss << s.rdbuf();
    ASSERT_EQ(ss.str(), "\xff\xfe\x01\xff\xff\xfe\xff\xfe\0"_v);
}

TEST(Output, unescaping) {
    auto r = "\xff\xfe\x01\xff\xff\xfe\xff\xfe\0"_v;

    std::vector<uint8_t> buf;
    {
        Output o(buf);
        o = Output(o, InputRestrictions({0}, true));
        auto s = o.as_stream();
        s.write((const char*) r.data(), r.size());
    }
    ASSERT_EQ(View(buf), "\0\x01\xff\xfe\0"_v);
}

TEST(Input, escaping_view_not) {
    Input i("\0\x01\xff\xfe\0"_v);
    auto v = i.as_view();
    ASSERT_EQ(View(v), "\0\x01\xff\xfe\0"_v);
}

TEST(Input, escaping_stream_not) {
    Input i("\0\x01\xff\xfe\0"_v);
    auto s = i.as_stream();
    std::stringstream ss;
    ss << s.rdbuf();
    ASSERT_EQ(ss.str(), "\0\x01\xff\xfe\0"_v);
}

TEST(Output, unescaping_not) {
    auto r = "\xff\xfe\x01\xff\xff\xfe\xff\xfe\0"_v;

    std::vector<uint8_t> buf;
    {
        Output o(buf);
        auto s = o.as_stream();
        s.write((const char*) r.data(), r.size());
    }
    ASSERT_EQ(View(buf), "\xff\xfe\x01\xff\xff\xfe\xff\xfe\0"_v);
}

TEST(Output, memory) {
    std::vector<uint8_t> vec;

    Output out = Output::from_memory(vec);

    {
        auto stream = out.as_stream();

        stream << "abc";
    }

    ASSERT_EQ(vec, (std::vector<uint8_t> { 97, 98, 99 }));
}

TEST(Output, file) {
    Output out = Output::from_path(io::Path(test::test_file_path(fn("short_out.txt"))), true);

    {
        auto stream = out.as_stream();

        stream << "abc";
    }

    ASSERT_EQ(test::read_test_file(fn("short_out.txt")), "abc");
}

TEST(Output, stream) {
    std::stringstream ss;
    Output out = Output::from_stream(ss);

    {
        auto stream = out.as_stream();

        stream << "abc";
    }

    ASSERT_EQ(ss.str(), "abc");
}

TEST(Input, file_not_exists_stream) {
    bool threw = false;
    try {
        Input ft = Input::from_path("asdfgh.txt");
        ft.as_stream();
    } catch (std::runtime_error& e) {
        ASSERT_EQ(View(e.what()), "input file asdfgh.txt does not exist");
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Input, file_not_exists_view) {
    bool threw = false;
    try {
        Input ft = Input::from_path("asdfgh.txt");
        ft.as_view();
    } catch (std::runtime_error& e) {
        ASSERT_EQ(View(e.what()), "input file asdfgh.txt does not exist");
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(Output, file_not_exists_view) {
    bool threw = false;
    try {
        Output ft = Output::from_path(io::Path("asdfgh/out.txt"));
        ft.as_stream();
    } catch (std::runtime_error& e) {
        ASSERT_EQ(View(e.what()), "output file asdfgh/out.txt can not be created/accessed");
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(View, construction) {
    static const uint8_t DATA[3] = { 'f', 'o', 'o' };

    View a(DATA, sizeof(DATA));
    ASSERT_EQ(a.size(), 3u);
    ASSERT_EQ(a[0], 'f');
    ASSERT_EQ(a[2], 'o');

    View b(a);
    ASSERT_EQ(b.size(), 3u);
    ASSERT_EQ(b[0], 'f');
    ASSERT_EQ(b[2], 'o');

    View c(std::move(a));
    ASSERT_EQ(c.size(), 3u);
    ASSERT_EQ(c[0], 'f');
    ASSERT_EQ(c[2], 'o');

    std::vector<uint8_t> vec { 'b', 'a', 'r' };
    View d(vec);
    ASSERT_EQ(d.size(), 3u);
    ASSERT_EQ(d[0], 'b');
    ASSERT_EQ(d[2], 'r');

    View e("bar");
    ASSERT_EQ(e.size(), 3u);
    ASSERT_EQ(e[0], 'b');
    ASSERT_EQ(e[2], 'r');

    std::string str("bar");
    View f(str);
    ASSERT_EQ(f.size(), 3u);
    ASSERT_EQ(f[0], 'b');
    ASSERT_EQ(f[2], 'r');

    View g("qux!!", 3);
    ASSERT_EQ(g.size(), 3u);
    ASSERT_EQ(g[0], 'q');
    ASSERT_EQ(g[2], 'x');
}

TEST(View, conversion) {
    View s("xyz");

    std::string a(s);
    ASSERT_EQ(a, "xyz");

    std::vector<uint8_t> b(s);
    ASSERT_EQ(b, (std::vector<uint8_t> { 'x', 'y', 'z' }));
}

TEST(View, indexing) {
    const char* s = "abcde";

    View a(s);
    ASSERT_EQ(a.at(0), 'a');
    ASSERT_EQ(a.at(4), 'e');
    ASSERT_EQ(a[0], 'a');
    ASSERT_EQ(a[4], 'e');

    try {
        a.at(5);
    } catch (std::out_of_range& e) {
        ASSERT_EQ(std::string(e.what()),
                  "accessing view with bounds [0, 5) at out-of-bounds index 5");
    }

#ifdef DEBUG
    try {
        a[5];
    } catch (std::out_of_range& e) {
        ASSERT_EQ(std::string(e.what()),
                  "accessing view with bounds [0, 5) at out-of-bounds index 5");
    }
#endif

    ASSERT_EQ(a.front(), 'a');
    ASSERT_EQ(a.back(), 'e');
    ASSERT_EQ(a.data(), (const uint8_t*) s);
}

TEST(View, iterators) {
    View s("abc");

    std::vector<uint8_t> a (s.begin(), s.end());
    ASSERT_EQ(a, (std::vector<uint8_t> { 'a', 'b', 'c' }));

    std::vector<uint8_t> b (s.cbegin(), s.cend());
    ASSERT_EQ(b, (std::vector<uint8_t> { 'a', 'b', 'c' }));

    std::vector<uint8_t> c (s.rbegin(), s.rend());
    ASSERT_EQ(c, (std::vector<uint8_t> { 'c', 'b', 'a' }));

    std::vector<uint8_t> d (s.crbegin(), s.crend());
    ASSERT_EQ(d, (std::vector<uint8_t> { 'c', 'b', 'a' }));
}

TEST(View, capacity) {
    View s("abc");

    ASSERT_EQ(s.empty(), false);
    ASSERT_EQ(s.size(), 3u);
    ASSERT_EQ(s.max_size(), 3u);

    View t("");

    ASSERT_EQ(t.empty(), true);
    ASSERT_EQ(t.size(), 0u);
    ASSERT_EQ(t.max_size(), 0u);
}

TEST(View, modifiers) {
    View a("abc");
    ASSERT_EQ(a.size(), 3u);

    View b("");
    ASSERT_EQ(b.size(), 0u);

    a.swap(b);
    ASSERT_EQ(a.size(), 0u);
    ASSERT_EQ(b.size(), 3u);

    swap(a, b);
    ASSERT_EQ(a.size(), 3u);
    ASSERT_EQ(b.size(), 0u);

    View c("asdf");
    c.remove_prefix(1);
    ASSERT_EQ(c, "sdf");
    c.remove_suffix(1);
    ASSERT_EQ(c, "sd");
    c.clear();
    ASSERT_EQ(c, "");
}

TEST(View, comparision) {
    View a("abc");
    View b("acc");
    View c("");

    ASSERT_TRUE(a == a);
    ASSERT_TRUE(c == c);

    ASSERT_TRUE(a != b);
    ASSERT_TRUE(a != c);

    ASSERT_TRUE(a < b);
    ASSERT_TRUE(a <= b);

    ASSERT_TRUE(b > a);
    ASSERT_TRUE(b >= a);

    ASSERT_TRUE(c < a);
    ASSERT_TRUE(c < b);
}

TEST(View, ostream) {
    std::stringstream ss;
    ss << View("abc");
    ASSERT_EQ(ss.str(), std::string("abc"));
}

TEST(View, slicing) {
    View a("abc");
    ASSERT_EQ(a.size(), 3u);
    ASSERT_EQ(a, "abc");

    View b = a.slice(0, 3);
    ASSERT_EQ(b.size(), 3u);
    ASSERT_EQ(b, "abc");

    View c = a.slice(0);
    ASSERT_EQ(c.size(), 3u);
    ASSERT_EQ(c, "abc");

    View d = a.slice(2);
    ASSERT_EQ(d.size(), 1u);
    ASSERT_EQ(d, "c");

    View e = a.slice(2, 3);
    ASSERT_EQ(e.size(), 1u);
    ASSERT_EQ(e, "c");

    View f = a.slice(1, 2);
    ASSERT_EQ(f.size(), 1u);
    ASSERT_EQ(f, "b");
}

TEST(View, substring) {
    View a("abc");
    ASSERT_EQ(a.size(), 3u);
    ASSERT_EQ(a, "abc");

    View b = a.substr(0, 3);
    ASSERT_EQ(b.size(), 3u);
    ASSERT_EQ(b, "abc");

    View c = a.substr(0);
    ASSERT_EQ(c.size(), 3u);
    ASSERT_EQ(c, "abc");

    View d = a.substr(2);
    ASSERT_EQ(d.size(), 1u);
    ASSERT_EQ(d, "c");

    View e = a.substr(2, 1);
    ASSERT_EQ(e.size(), 1u);
    ASSERT_EQ(e, "c");

    View f = a.substr(1, 1);
    ASSERT_EQ(f.size(), 1u);
    ASSERT_EQ(f, "b");
}

TEST(View, slice_vs_substr) {
    View a("abcde");

    ASSERT_EQ(a.slice(0), a.substr(0));
    ASSERT_EQ(a.slice(3), a.substr(3));

    ASSERT_EQ(a.slice(0, 3), a.substr(0, 3));
    ASSERT_EQ(a.slice(3, 5), a.substr(3, 2));
}

TEST(View, string_predicates) {
    View a("abc");
    View aa("abcabc");

    ASSERT_TRUE(a.starts_with('a'));
    ASSERT_TRUE(a.starts_with(uint8_t('a')));
    ASSERT_TRUE(a.ends_with('c'));
    ASSERT_TRUE(a.ends_with(uint8_t('c')));

    ASSERT_TRUE(a.starts_with(a));
    ASSERT_TRUE(a.ends_with(a));
    ASSERT_TRUE(aa.starts_with(a));
    ASSERT_TRUE(aa.ends_with(a));
}

TEST(View, literal) {
    ASSERT_EQ("abc"_v, View("abc"));
    ASSERT_NE("abc"_v, "abc\0def\0"_v);
    ASSERT_EQ("abc\0def\0"_v, "abc\0def\0"_v);
    ASSERT_NE("abc\0def\0"_v, "abc"_v);

    ASSERT_EQ("a\0"_v.back(), 0);
}

template<class V, class ConstV>
void view_test_template_const() {
    using T = typename V::value_type;

    const V v;
    const ConstV cv;

    T data1[3] = { 0, 1, 2 };
    const V constr1 { &data1[0], 3 };

    const V copy1 { constr1 };

    const ConstV copy2 { copy1 };

    std::vector<T> data2 { 1, 2, 3 };
    const V copy3 { data2 };

    std::vector<T> copy4 = constr1;

    std::vector<T> data3 { 3, 4, 5 };
    const V v2 { data3 };
    const ConstV cv2 { data3 };

    ConstV cv3;
    cv3 = v2;
    cv3 = cv2;

    v.begin();
    v.end();
    v.rbegin();
    v.rend();
    v.cbegin();
    v.cend();
    v.crbegin();
    v.crend();

    v.size();
    v.max_size();
    v.empty();

    copy1[0];
    copy1.at(0);

    copy1.front();
    copy1.back();
    copy1.data();

    V pop_copy { data2 };
    V swap_copy { data2 };

    pop_copy.pop_back();
    pop_copy.pop_front();

    pop_copy.swap(swap_copy);
    pop_copy.clear();

    const V slice_copy { data2 };
    const V slice1 = slice_copy.substr(0);
    const V slice2 = slice_copy.substr(0, 1);
    slice1.empty();
    slice2.empty();

    pop_copy.remove_prefix(0);
    pop_copy.remove_suffix(0);

    const ConstV substr_copy1 { slice_copy };

    substr_copy1.starts_with(v);
    substr_copy1.ends_with(v);

    const V substr_copy2 { slice_copy };

    substr_copy2.starts_with(v);
    substr_copy2.ends_with(v);
    substr_copy2.starts_with(cv);
    substr_copy2.ends_with(cv);
    substr_copy2.starts_with(0);
    substr_copy2.ends_with(0);

    ASSERT_TRUE( v == v);
    ASSERT_FALSE(v != v);
    ASSERT_FALSE(v <  v);
    ASSERT_TRUE( v <= v);
    ASSERT_FALSE(v >  v);
    ASSERT_TRUE( v >= v);

    V swap1;
    V swap2;

    using std::swap;
    swap(swap1, swap2);

    ASSERT_TRUE( v == cv);
    ASSERT_FALSE(v != cv);
    ASSERT_FALSE(v <  cv);
    ASSERT_TRUE( v <= cv);
    ASSERT_FALSE(v >  cv);
    ASSERT_TRUE( v >= cv);

    ASSERT_TRUE( cv == v);
    ASSERT_FALSE(cv != v);
    ASSERT_FALSE(cv <  v);
    ASSERT_TRUE( cv <= v);
    ASSERT_FALSE(cv >  v);
    ASSERT_TRUE( cv >= v);

}

template<class V, class ConstV>
void view_test_template() {
    using T = typename V::value_type;

    V v;
    const ConstV cv;

    std::vector<T> data { 1, 2, 3 };
    V copy1 { data };

    {
        auto a = copy1.begin();       *a = 0;
        auto b = copy1.end();    --b; *b = 0;
        auto c = copy1.rbegin();      *c = 0;
        auto d = copy1.rend();   --d; *d = 0;
    }

    copy1[0] = 0;
    copy1.at(0) = 0;
    copy1.front() = 0;
    copy1.back() = 0;
    *copy1.data() = 0;

}

template<class V, class ConstV>
void view_test_template_const_8() {
    const V v;
    const ConstV cv;

    std::string data1 { "abc" };
    const V copy1 { data1 };

    const char* data2 = "abc";
    const V copy2 { data2 };
    const V copy3 { data2, 1 };

    std::string copy4 = copy1;

}

template<class T>
void view_test_template_simple() {
    view_test_template_const<ConstGenericView<T>, ConstGenericView<T>>();
    view_test_template_const<     GenericView<T>, ConstGenericView<T>>();
    view_test_template<           GenericView<T>, ConstGenericView<T>>();
}

template<>
void view_test_template_simple<uint8_t>() {
    using T = uint8_t;
    view_test_template_const<ConstGenericView<T>, ConstGenericView<T>>();
    view_test_template_const<     GenericView<T>, ConstGenericView<T>>();
    view_test_template<           GenericView<T>, ConstGenericView<T>>();
    view_test_template_const_8<ConstGenericView<T>, ConstGenericView<T>>();
}

TEST(GenericView, template_8) {
    view_test_template_simple<uint8_t>();
}
TEST(GenericView, template_16) {
    view_test_template_simple<uint16_t>();
}
TEST(GenericView, template_32) {
    view_test_template_simple<uint32_t>();
}
TEST(GenericView, template_64) {
    view_test_template_simple<uint64_t>();
}

struct MySubAlgo: Algorithm {
    MySubAlgo(Config&& c): Algorithm(std::move(c)) {}

    inline static Meta meta() {
        Meta y(TypeDesc("sub_t"), "sub1");
        y.param("x").primitive("x");
        return y;
    }
};

struct MySubAlgo2: Algorithm {
    MySubAlgo2(Config&& c): Algorithm(std::move(c)) {}

    inline static Meta meta() {
        Meta y(TypeDesc("sub_t"), "sub2");
        y.param("y").primitive("y");
        return y;
    }
};

template<class A>
struct MyCompressor: public Compressor {
    inline static Meta meta() {
        Meta y(Compressor::type_desc(), "my");
        y.param("sub").strategy<A>(TypeDesc("sub_t"), Meta::Default<MySubAlgo2>());
        y.param("dyn").primitive("foobar");
        y.param("bool_val").primitive(true);
        return y;
    }

    std::string custom_data;
    MyCompressor() = delete;
    MyCompressor(Config&& cfg, std::string&& s):
        Compressor(std::move(cfg)),
        custom_data(std::move(s)) {}

    inline virtual void compress(Input&, Output& output) {
        A a(config().sub_config("sub"));
        auto s = output.as_stream();
        s << "ok! " << custom_data << " " << config().param("dyn").as_string();
        ASSERT_TRUE(config().param("bool_val").as_bool());
    }

    inline virtual std::unique_ptr<Decompressor> decompressor() const override {
        throw std::runtime_error("not implemented");
    }
};

TEST(Algorithm, create) {
    auto x = Algorithm::instance<MyCompressor<MySubAlgo>>("", "test");

    std::vector<uint8_t> vec;
    Output out(vec);
    Input inp("");
    x->compress(inp, out);

    auto s = vec_as_lossy_string(vec);

    ASSERT_EQ(s, "ok! test foobar");
}

TEST(Test, TestInputCompression) {
    test::TestInput i = test::compress_input("abcd"_v);
    ASSERT_EQ(i.as_view(), "abcd\0"_v);
}

TEST(Test, TestOutputCompression) {
    test::TestOutput o = test::compress_output();
    o.as_stream() << "abcd\0"_v;
    ASSERT_EQ(o.result(), "abcd\0"_v);
}

TEST(Test, TestInputDecompression) {
    test::TestInput i = test::decompress_input("abcd\0"_v);
    ASSERT_EQ(i.as_view(), "abcd\0"_v);
}

TEST(Test, TestOutputDecompression) {
    test::TestOutput o = test::decompress_output();
    o.as_stream() << "abcd\0"_v;
    ASSERT_EQ(o.result(), "abcd"_v);
}

TEST(Test, TestInputCompressionFile) {
    test::write_test_file("TestInputCompressionFile.txt", "abcd");
    auto p = test::test_file_path("TestInputCompressionFile.txt");
    test::TestInput i = test::compress_input_file(p);
    ASSERT_EQ(i.as_view(), "abcd\0"_v);
}

TEST(Test, TestInputDecompressionFile) {
    test::write_test_file("TestInputDecompressionFile.txt", "abcd\0"_v);
    auto p = test::test_file_path("TestInputDecompressionFile.txt");
    test::TestInput i = test::decompress_input_file(p);
    ASSERT_EQ(i.as_view(), "abcd\0"_v);
}

TEST(Test, TestInputOutputInheritance) {
    auto x = Algorithm::instance<MyCompressor<MySubAlgo>>("", "test");

    auto i = test::compress_input("asdf");
    auto o = test::compress_output();

    x->compress(i, o);
}

TEST(MMapHandle, test1) {
    auto s = "asdkfjkasldlasdkflhkasddklashldasldkjalskdd"_v;
    test::write_test_file("mmap1.txt", s);
    auto path = test::test_file_path("mmap1.txt");

    const MMap mmap { path, MMap::Mode::Read, s.size() };

    ASSERT_EQ(mmap.view(), s);
}

TEST(Test, compress_input) {
    auto i = test::compress_input("ab\0cd\0"_v);

    ASSERT_EQ(i.as_view(), "ab\xff\xfe""cd\xff\xfe\0"_v);
    std::stringstream ss;
    ss << i.as_stream().rdbuf();
    ASSERT_EQ(View(ss.str()), "ab\xff\xfe""cd\xff\xfe\0"_v);
}

TEST(Test, compress_output) {
    auto o = test::compress_output();
    o.as_stream() << "ab\0cd\0"_v;
    ASSERT_EQ(o.result(), "ab\0cd\0"_v);
}

TEST(Test, decompress_input) {
    auto i = test::decompress_input("ab\0cd\0"_v);

    ASSERT_EQ(i.as_view(), "ab\0cd\0"_v);
    std::stringstream ss;
    ss << i.as_stream().rdbuf();
    ASSERT_EQ(View(ss.str()), "ab\0cd\0"_v);
}

TEST(Test, decompress_output) {
    auto o = test::decompress_output();
    o.as_stream() << "ab\xff\xfe""cd\xff\xfe\0"_v;
    ASSERT_EQ(o.result(), "ab\0cd\0"_v);
}

TEST(Util, power_of_two) {
    ASSERT_EQ(zero_or_next_power_of_two(0), 0U);
    ASSERT_EQ(zero_or_next_power_of_two(1), 1U);
    ASSERT_EQ(zero_or_next_power_of_two(2), 2U);
    ASSERT_EQ(zero_or_next_power_of_two(3), 4U);
    ASSERT_EQ(zero_or_next_power_of_two(4), 4U);
    ASSERT_EQ(zero_or_next_power_of_two(5), 8U);
    ASSERT_EQ(zero_or_next_power_of_two(6), 8U);
    ASSERT_EQ(zero_or_next_power_of_two(7), 8U);
    ASSERT_EQ(zero_or_next_power_of_two(8), 8U);
}

TEST(Util, parse_bytes) {
    ASSERT_EQ(100ULL, parse_bytes("100"));
    ASSERT_EQ(322122547200ULL, parse_bytes("300Gi"));
    ASSERT_EQ(209715200ULL, parse_bytes("200Mi"));
    ASSERT_EQ(150528ULL, parse_bytes("147Ki"));
    ASSERT_EQ(147ULL, parse_bytes("147Bi"));
    ASSERT_EQ(300000000000ULL, parse_bytes("300G"));
    ASSERT_EQ(200000000ULL, parse_bytes("200M"));
    ASSERT_EQ(147000ULL, parse_bytes("147K"));
    ASSERT_EQ(147ULL, parse_bytes("147B"));
}
