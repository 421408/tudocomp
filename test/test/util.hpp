#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <memory>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <sys/stat.h>

#include <tudocomp/tudocomp.hpp>
#include <tudocomp/Env.hpp>
#include <tudocomp/Compressor.hpp>
#include <tudocomp/Algorithm.hpp>
#include <tudocomp/AlgorithmStringParser.hpp>
#include <tudocomp/Registry.hpp>
#include <tudocomp/io.hpp>
#include <tudocomp/util/View.hpp>

using namespace tdc;

namespace test {

// TODO: Actually specialize the 3 kinds

/// Error diagnostic optimized for string data
template<class T, class U>
void assert_eq_strings(const T& expected_, const U& actual_) {
    std::string expected(expected_.begin(), expected_.end());
    std::string actual(actual_.begin(), actual_.end());

    ASSERT_EQ(expected, actual);
}

/// Error diagnostic optimized for binary data
template<class T, class U>
void assert_eq_integers(const T& expected_, const U& actual_) {
    std::vector<uint64_t> expected(expected_.begin(), expected_.end());
    std::vector<uint64_t> actual(actual_.begin(), actual_.end());

    ASSERT_EQ(expected, actual);
}

/// Error diagnostic optimized for mixed binary/ascii data
template<class T, class U>
void assert_eq_hybrid_strings(const T& expected, const U& actual) {
    ASSERT_EQ(expected, actual);
}

/// Error diagnostic optimized for arbitrary data
template<class T, class U>
void assert_eq_sequence(const T& expected, const U& actual) {
    ASSERT_EQ(expected.size(), actual.size()) << "assert_eq_sequence: sizes differ";
    for (size_t i = 0; i < expected.size(); i++)
        ASSERT_EQ(expected[i], actual[i]) << "assert_eq_sequence: failed at i=" << i;
}

/// Temporary provides a `ostream` to write into, and returns it as a string.
///
/// This is useful for testing Coder::code() and Coder::decode().
///
/// \param f A callable type (like for example a C++ lambda expression)
///          that receives an std::ostream& as an argument so that its
///          body can write into it.
template<class Lambda>
std::string ostream_to_string(Lambda f) {
    std::stringstream ss;
    std::ostream& os = ss;
    f(os);
    return ss.str();
}

/// Temporary provides a `ostream` to write into, and returns it as a
/// byte vector.
///
/// This is useful for testing Coder::code() and Coder::decode().
///
/// \param f A callable type (like for example a C++ lambda expression)
///          that receives an std::ostream& as an argument so that its
///          body can write into it.
template<class Lambda>
std::vector<uint8_t> ostream_to_bytes(Lambda f) {
    auto s = ostream_to_string(f);
    return std::vector<uint8_t>(s.begin(), s.end());
}

/// Call the given function with a number
/// of different strings testing common corner cases and unicode input.
template<class F>
void roundtrip_batch(F f) {
    f("abcdebcdeabc"_v);
    f("a"_v);
    f(""_v);

    f("aaaaaaaaa"_v); \
    f("banana"_v); \
    f("ananas"_v); \
    f("abcdefgh#defgh_abcde"_v); \

    f("abcdebcdeabcd"_v);
    f("foobar"_v);
    f("abcabcabcabc"_v);

    f("abc abc  abc"_v);

    f("abaaabbababb"_v);

    f(
        "asdfasctjkcbweasbebvtiwetwcnbwbbqnqxernqzezwuqwezuet"
        "qcrnzxbneqebwcbqwicbqcbtnqweqxcbwuexcbzqwezcqbwecqbw"
        "dassdasdfzdfgfsdfsdgfducezctzqwebctuiqwiiqcbnzcebzqc"_v);

    f("ประเทศไทย中华Việt Nam"_v);

    f(
        "Lorem ipsum dolor sit amet, sea ut etiam solet salut"
        "andi, sint complectitur et his, ad salutandi imperdi"
        "et gubergren per mei."_v);

    f(
        "Лорэм атоморюм ут хаж, эа граэки емпыдит ёудёкабет "
        "мэль, декам дежпютатионй про ты. Нэ ёужто жэмпэр"
        " жкрибэнтур векж, незл коррюмпит."_v);

    f(
        "報チ申猛あち涙境ワセ周兵いわ郵入せすをだ漏告されて話巡わッき"
        "や間紙あいきり諤止テヘエラ鳥提フ健2銀稿97傷エ映田ヒマ役請多"
        "暫械ゅにうて。関国ヘフヲオ場三をおか小都供セクヲ前俳著ゅ向深"
        "まも月10言スひす胆集ヌヱナ賀提63劇とやぽ生牟56詰ひめつそ総愛"
        "ス院攻せいまて報当アラノ日府ラのがし。"_v);

    f(
        "Εαμ ανσιλλαε περισυλα συαφιθαθε εξ, δυο ιδ ρεβυμ σομ"
        "μοδο. Φυγιθ ηομερω ιυς ατ, ει αυδιρε ινθελλεγαμ νες."
        " Ρεκυε ωμνιυμ μανδαμυς κυο εα. Αδμοδυμ σωνσεκυαθ υθ "
        "φιξ, εσθ ετ πρωβατυς συαφιθαθε ραθιονιβυς, ταντας αυ"
        "διαμ ινστρυσθιορ ει σεα."_v);

    f("struct Foo { uint8_t bar }"_v);

    f("ABBCBCABA"_v);

    f("abcabca"_v);

    f("abbbbbbbbbbcbbbbbbbbbb"_v);

    //f("abc\0"_v);

    std::vector<uint8_t> all_bytes {
        0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,
        16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
        32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
        48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
        64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
        80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
        96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,
    };

    //f(View(all_bytes));
}

#include <tudocomp/util/Generators.hpp>
template<class F>
void on_string_generators(F func, size_t n) {
    for(size_t i = 0; i < n; ++i) {
        std::string s = fibonacci_word(i);
        func(s);
    }
    for(size_t i = 0; i < n; ++i) {
        std::string s = thue_morse_word(i);
        func(s);
    }
    for(size_t i = 0; i < n; ++i) {
        std::string s = run_rich(i);
        func(s);
    }
    for(size_t i = 2; i < n; ++i) {
        for(size_t j = 0; j < 2+50/(i+1); ++j) {
            std::string s = random_uniform(1<<i,Ranges::numbers,j);
            func(s);
        }
    }
}

const std::string TEST_FILE_PATH = "test_files";

inline std::string test_file_path(const std::string& filename) {
    return TEST_FILE_PATH + "/" + filename;
}

inline bool test_file_exists(const std::string& filename) {
    std::string test_file_name = test_file_path(filename);

    struct stat buf;
    return (stat(test_file_name.c_str(), &buf) == 0);
}

inline std::string read_test_file(const std::string& filename) {
    std::ostringstream sout;

    std::string test_file_name = test_file_path(filename);
    std::ifstream fin(test_file_name);
    if(fin) {
        sout << fin.rdbuf();

        fin.close();
    } else {
        std::string msg = "Could not open test file \"";
        msg += test_file_name;
        msg += "\"";
        throw std::runtime_error(msg);
    }

    auto s = sout.str();

    return s;
}

inline void create_test_directory() {
    mkdir(TEST_FILE_PATH.c_str(), 0777);
}

inline void write_test_file(const std::string& filename, string_ref text) {
    create_test_directory();
    std::ofstream fout(test_file_path(filename));
    if(fout) {
        fout << text;
        fout.close();
    }
}

inline void remove_test_file(const std::string& filename) {
    create_test_directory();
    remove(test_file_path(filename).c_str());
}

inline std::vector<uint8_t> pack_integers(std::vector<uint64_t> ints) {
    CHECK(ints.size() % 2 == 0);
    std::vector<uint8_t> bits;

    uint bit_pos = 8;
    for (size_t i = 0; i < ints.size(); i += 2) {
        uint64_t val = ints[i];
        uint64_t val_bits = ints[i + 1];
        for (uint64_t bit = 0; bit < val_bits; bit++) {
            if (bit_pos == 8) {
                bits.push_back(0);
                bit_pos = 0;
            }

            uint8_t& b = bits[bits.size() - 1];
            if (val & (uint64_t(1) << (val_bits - bit - 1))) {
                b |= (1 << (7 - bit_pos));
            }

            bit_pos++;
        }
    }

    return bits;
}

template<class C>
struct CompressResult {
private:
    Registry m_registry;
public:
    std::vector<uint8_t> bytes;
    std::string str;
    std::string orginal_text;
    std::string options;

    CompressResult(const Registry& registry,
                    std::vector<uint8_t>&& p_bytes,
                    std::string&& p_str,
                    std::string&& p_original,
                    std::string&& p_options):
        m_registry(registry),
        bytes(std::move(p_bytes)),
        str(std::move(p_str)),
        orginal_text(std::move(p_original)),
        options(std::move(p_options)) {}

    void assert_decompress() {
        std::vector<uint8_t> decoded_buffer;
        {
            Input text_in = Input::from_memory(bytes);
            Output decoded_out = Output::from_memory(decoded_buffer);

            auto compressor = create_algo_with_registry<C>(options, m_registry);

            if (C::meta().is_needs_sentinel_terminator()) {
                decoded_out.unescape_and_trim();
            }

            compressor.decompress(text_in, decoded_out);
        }
        std::string decompressed_text {
            decoded_buffer.begin(),
            decoded_buffer.end(),
        };
        ASSERT_EQ(orginal_text, decompressed_text);
    }

    void assert_decompress_bytes() {
        std::vector<uint8_t> decompressed_bytes;
        {
            Input text_in = Input::from_memory(bytes);
            Output decoded_out = Output::from_memory(decompressed_bytes);

            auto compressor = create_algo_with_registry<C>(options, m_registry);

            if (C::meta().is_needs_sentinel_terminator()) {
                decoded_out.unescape_and_trim();
            }

            compressor.decompress(text_in, decoded_out);
        }
        std::vector<uint8_t> orginal_bytes {
            orginal_text.begin(),
            orginal_text.end(),
        };
        ASSERT_EQ(orginal_bytes, decompressed_bytes);
    }
};

template<class C>
class RoundTrip {
    std::string m_options;
    Registry m_registry;
public:
    inline RoundTrip(const std::string& options = "",
                        const Registry& registry = Registry()):
        m_options(options),
        m_registry(registry)
    {
    }

    CompressResult<C> compress(string_ref text) {
        std::vector<uint8_t> encoded_buffer;
        {
            Input text_in = Input::from_memory(text);
            Output encoded_out = Output::from_memory(encoded_buffer);

            auto compressor = create_algo_with_registry<C>(m_options, m_registry);
            if (C::meta().is_needs_sentinel_terminator()) {
                text_in.escape_and_terminate();
            }
            compressor.compress(text_in, encoded_out);
        }
        std::string s(encoded_buffer.begin(), encoded_buffer.end());
        return CompressResult<C> {
            m_registry,
            std::move(encoded_buffer),
            std::move(s),
            std::string(text),
            std::string(m_options),
        };
    }
};

template<class T>
inline CompressResult<T> compress(string_ref text,
                                    const std::string& options = "",
                                    const Registry& registry = Registry()) {
    return RoundTrip<T>(options, registry).compress(text);
}

template<class T>
inline void roundtrip(string_ref original_text,
                        string_ref expected_compressed_text = "",
                        const std::string& options = "",
                        const Registry& registry = Registry()) {
    auto e = RoundTrip<T>(options, registry).compress(original_text);
    auto& compressed_text = e.str;

    if(expected_compressed_text.size() > 0)
    ASSERT_EQ(std::string(expected_compressed_text), compressed_text);

    e.assert_decompress();
}

template<class T>
inline void roundtrip(string_ref original_text,
                        const std::vector<uint8_t>& expected_compressed_text,
                        const std::string& options = "",
                        const Registry& registry = Registry()) {
    auto e = RoundTrip<T>(options, registry).compress(original_text);
    auto& compressed_text = e.bytes;

    ASSERT_EQ(expected_compressed_text, compressed_text);

    e.assert_decompress_bytes();
}

class TestInput: public Input {
public:
    inline TestInput(string_ref text, bool sentinel): Input(text) {
        if (sentinel) {
            this->escape_and_terminate();
        }
    }
    inline TestInput(Input::Path&& path, bool sentinel): Input(std::move(path)) {
        if (sentinel) {
            this->escape_and_terminate();
        }
    }
};

class TestOutput: std::vector<uint8_t>, public Output {
public:
    inline TestOutput(bool sentinel): std::vector<uint8_t>(),
        Output(static_cast<std::vector<uint8_t>&>(*this))
    {
        if (sentinel) {
            this->unescape_and_trim();
        }
    }

    inline string_ref result() { return *this; }
};

/// Creates an instance of an `tdc::Input` to be used with `Compressor::compress()`.
///
/// It will contain the bytes given by `text`.
TestInput compress_input(string_ref text) {
    return TestInput(text, true);
}

/// Creates an instance of an `tdc::Input` to be used with `Compressor::compress()`.
///
/// It will contain the bytes contained in the file at `path`.
TestInput compress_input_file(string_ref path) {
    return TestInput(Input::Path{std::string(path)}, true);
}

/// Creates an instance of an `tdc::Output` to be used with `Compressor::compress()`.
///
/// The bytes output to it are accessible with the `.result()` accessor.
TestOutput compress_output() {
    return TestOutput(false);
}

/// Creates an instance of an `tdc::Input` to be used with `Compressor::decompress()`.
///
/// It will contain the bytes given by `text`.
TestInput decompress_input(string_ref text) {
    return TestInput(text, false);
}

/// Creates an instance of an `tdc::Input` to be used with `Compressor::decompress()`.
///
/// It will contain the bytes contained in the file at `path`.
TestInput decompress_input_file(string_ref path) {
    return TestInput(Input::Path{std::string(path)}, false);
}

/// Creates an instance of an `tdc::Output` to be used with `Compressor::decompress()`.
///
/// The bytes output to it are accessible with the `.result()` accessor.
TestOutput decompress_output() {
    return TestOutput(true);
}

}
