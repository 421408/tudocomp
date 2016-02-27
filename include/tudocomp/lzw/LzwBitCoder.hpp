#ifndef _INCLUDED_LZW_BIT_CODER_HPP_
#define _INCLUDED_LZW_BIT_CODER_HPP_

#include <tudocomp/Coder.hpp>

#include <tudocomp/lz78/trie.h>
#include <tudocomp/lz78/Lz78DecodeBuffer.hpp>

#include <tudocomp/lzw/factor.h>
#include <tudocomp/lzw/decode.hpp>

namespace tudocomp {

namespace lzw {

using ::lzw::LzwEntry;

/**
 * Encodes factors as simple strings.
 */
class LzwBitCoder {
private:
    // TODO: Change encode_* methods to not take Output& since that inital setup
    // rather, have a single init location
    tudocomp::output::StreamGuard m_out_guard;
    tudocomp::BitOstream m_out;
    uint64_t m_factor_counter = 0;

public:
    inline LzwBitCoder(Env& env, Output& out)
        : m_out_guard(out.as_stream()), m_out(*m_out_guard)
    {
    }

    inline LzwBitCoder(Env& env, Output& out, size_t len)
        : m_out_guard(out.as_stream()), m_out(*m_out_guard)
    {
    }

    inline ~LzwBitCoder() {
        m_out.flush();
        (*m_out_guard).flush();
    }

    inline void encode_sym(uint8_t sym) {
        throw std::runtime_error("encoder does not support encoding raw symbols");
    }

    inline void encode_fact(const LzwEntry& entry) {
        // output format: variable_number_backref_bits 8bit_char

        // slowly grow the number of bits needed together with the output
        size_t back_ref_idx_bits = bitsFor(m_factor_counter + 256);

        DCHECK(bitsFor(entry) <= back_ref_idx_bits);

        m_out.write(entry, back_ref_idx_bits);

        m_factor_counter++;
    }

    inline static void decode(Input& _inp, Output& _out) {
        auto iguard = _inp.as_stream();
        auto oguard = _out.as_stream();
        auto& inp = *iguard;
        auto& out = *oguard;

        bool done = false;
        BitIstream is(inp, done);

        uint64_t counter = 0;
        decode_step([&]() -> LzwEntry {
            // Try to read next factor
            LzwEntry factor(is.readBits<uint64_t>(bitsFor(counter + 256)));
            if (done) {
                // Could not read all bits -> done
                // (this works because the encoded factors are always > 8 bit)
                return LzwEntry(-1);
            }
            counter++;
            return factor;
        }, out);
    }
};

}

}

#endif
