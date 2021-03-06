#include <gtest/gtest.h>

#include <tudocomp/Compressor.hpp>
#include <tudocomp/Generator.hpp>

#include <tudocomp/compressors/lzss/LZSSCoding.hpp>
#include <tudocomp/compressors/lzss/DecompBackBuffer.hpp>
#include <tudocomp/compressors/lzss/FactorBuffer.hpp>
#include <tudocomp/compressors/lzss/UnreplacedLiterals.hpp>

#include <tudocomp/compressors/lcpcomp/decompress/CompactDec.hpp>
#include <tudocomp/compressors/lcpcomp/decompress/DecodeQueueListBuffer.hpp>
#include <tudocomp/compressors/lcpcomp/decompress/MultiMapBuffer.hpp>

using namespace tdc;

using factorbuffer_t = lzss::FactorBuffer<>;

TEST(lzss, factor_buffer_empty) {
    factorbuffer_t buf;

    // empty buffer
    ASSERT_TRUE(buf.empty());
    ASSERT_EQ(0U, buf.size());
    ASSERT_TRUE(buf.is_sorted());
}

TEST(lzss, factor_buffer_sorted) {
    // sorted buffer
    const size_t n = 10;
    factorbuffer_t buf;

    for(size_t i = 0; i < n; i++) {
        buf.emplace_back(i, i + n, n + 1 - i);
    }

    ASSERT_FALSE(buf.empty());
    ASSERT_EQ(n, buf.size());
    ASSERT_TRUE(buf.is_sorted());
}

TEST(lzss, factor_buffer_sort) {
    // unsorted buffer
    const size_t n = 10;
    factorbuffer_t buf;

    for(size_t i = n; i > 0; i--) {
        buf.emplace_back(n + i, 2 * n + i, 2 * n - i);
    }

    ASSERT_FALSE(buf.is_sorted());

    buf.sort();
    ASSERT_TRUE(buf.is_sorted());

    auto last = buf.begin();
    for(auto next = last + 1; next != buf.end(); ++next) {
        ASSERT_LE(last->pos, next->pos);
        last = next;
    }
}

TEST(lzss, text_literals_empty) {
    factorbuffer_t empty;
    std::string tmp = "";
    lzss::UnreplacedLiterals<std::string, factorbuffer_t> literals(tmp, empty);
    ASSERT_FALSE(literals.has_next());
}

template<typename text_t>
void lzss_text_literals_factors(
    lzss::UnreplacedLiterals<text_t, factorbuffer_t>& literals,
    const std::string& ref_literals,
    const len_compact_t* ref_positions) {

    size_t i = 0;
    while(literals.has_next()) {
        auto l = literals.next();
        ASSERT_EQ(ref_literals[i],  l.c);
        ASSERT_EQ(ref_positions[i], l.pos);
        ++i;
    }

    ASSERT_EQ(ref_literals.length(), i);
}

TEST(lzss, text_literals_nofactors) {
    std::string text = "abcdefgh";
    const len_compact_t positions[] = {0,1,2,3,4,5,6,7};

    factorbuffer_t empty;
    lzss::UnreplacedLiterals<std::string, factorbuffer_t> literals(text, empty);

    lzss_text_literals_factors(literals, text, positions);
}

TEST(lzss, text_literals_factors_middle) {
    std::string text = "a__b____cd___e";
    std::string ref_literals = "abcde";
    const len_compact_t ref_positions[] = {0,3,8,9,13};

    factorbuffer_t factors;
    factors.emplace_back(1, text.length(), 2);
    factors.emplace_back(4, text.length(), 4);
    factors.emplace_back(10, text.length(), 3);

    lzss::UnreplacedLiterals<std::string, factorbuffer_t> literals(text, factors);

    lzss_text_literals_factors(literals, ref_literals, ref_positions);
}

TEST(lzss, text_literals_factors_begin) {
    std::string text = "___a__bc__de";
    std::string ref_literals = "abcde";
    const len_compact_t ref_positions[] = {3,6,7,10,11};

    factorbuffer_t factors;
    factors.emplace_back(0, text.length(), 3);
    factors.emplace_back(4, text.length(), 2);
    factors.emplace_back(8, text.length(), 2);

    lzss::UnreplacedLiterals<std::string, factorbuffer_t> literals(text, factors);

    lzss_text_literals_factors(literals, ref_literals, ref_positions);
}

TEST(lzss, text_literals_factors_end) {
    std::string text = "a___b__cd__e__";
    std::string ref_literals = "abcde";
    const len_compact_t ref_positions[] = {0,4,7,8,11};

    factorbuffer_t factors;
    factors.emplace_back(1, text.length(), 3);
    factors.emplace_back(5, text.length(), 2);
    factors.emplace_back(9, text.length(), 2);
    factors.emplace_back(12, text.length(), 2);

    lzss::UnreplacedLiterals<std::string, factorbuffer_t> literals(text, factors);

    lzss_text_literals_factors(literals, ref_literals, ref_positions);
}

TEST(lzss, decode_back_buffer) {
    lzss::DecompBackBuffer buffer;
    buffer.initialize(12);
    buffer.decode_literal('b');
    buffer.decode_literal('a');
    buffer.decode_literal('n');
    buffer.decode_factor(1, 3);
    buffer.decode_factor(0, 6);
    buffer.process();

    std::stringstream ss;
    buffer.write_to(ss);

    ASSERT_EQ("bananabanana", ss.str());
}

template<typename T>
void test_forward_decode_buffer_chain() {
    auto buffer = Algorithm::instance<T>();
    buffer->initialize(12);
    buffer->decode_literal('b');
    buffer->decode_factor(3, 3);
    buffer->decode_literal('n');
    buffer->decode_literal('a');
    buffer->decode_factor(0, 6);
    buffer->process();

    std::stringstream ss;
    buffer->write_to(ss);

    ASSERT_EQ("bananabanana", ss.str());
}

template<typename T>
void test_forward_decode_buffer_multiref() {
    auto buffer = Algorithm::instance<T>();
    buffer->initialize(12);
    buffer->decode_factor(6, 6);
    buffer->decode_literal('b');
    buffer->decode_factor(9, 3);
    buffer->decode_literal('n');
    buffer->decode_literal('a');
    buffer->process();

    std::stringstream ss;
    buffer->write_to(ss);

    ASSERT_EQ("bananabanana", ss.str());
}

TEST(lzss, decode_forward_lm_buffer_chain) {
    test_forward_decode_buffer_chain<lcpcomp::CompactDec>();
}

TEST(lzss, decode_forward_lm_buffer_multiref) {
    test_forward_decode_buffer_multiref<lcpcomp::CompactDec>();
}

TEST(lzss, decode_forward_mm_buffer_chain) {
    test_forward_decode_buffer_chain<lcpcomp::MultimapBuffer>();
}

TEST(lzss, decode_forward_mm_buffer_multiref) {
    test_forward_decode_buffer_multiref<lcpcomp::MultimapBuffer>();
}

TEST(lzss, decode_forward_ql_buffer_chain) {
    test_forward_decode_buffer_chain<lcpcomp::DecodeForwardQueueListBuffer>();
}

TEST(lzss, decode_forward_ql_buffer_multiref) {
    test_forward_decode_buffer_multiref<lcpcomp::DecodeForwardQueueListBuffer>();
}
