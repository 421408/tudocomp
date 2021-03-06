#pragma once

#include <tudocomp/compressors/lzss/FactorBuffer.hpp>
#include <tudocomp/compressors/lzss/LZSSCoder.hpp>

#include <tudocomp/Tags.hpp>

namespace tdc {
namespace lzss {

/// \brief Didactical coding strategy.
///
/// This strategy produces a LZSS-style human-readable output of the
// factorization.
class DidacticalCoder : public Algorithm {
public:
    inline static Meta meta() {
        Meta m(
            lzss_bidirectional_coder_type(),
            "didactic",
            "Didactical output of factors");
        m.add_tag(tags::lossy); // no decompression available!
        return m;
    }

    using Algorithm::Algorithm;

    class Encoder {
    private:
        std::unique_ptr<io::OutputStream> m_out;

    public:
        inline Encoder(const Config& cfg, Output& out) {
            m_out = std::make_unique<io::OutputStream>(out.as_stream());
        }

        inline void encode_header() {
        }

        inline void encode_factor(Factor f) {
            (*m_out) << '{' << f.src << ", " << f.len << '}';
        }

        inline void encode_literal(uliteral_t c) {
            (*m_out) << c;
        }

        template<typename text_t>
        inline void encode_run(const text_t& text, size_t p, const size_t q) {
            while(p < q) encode_literal(text[p++]);
        }

        template<typename text_t, typename factorbuffer_t>
        inline void encode_text(
            const text_t& text,
            const factorbuffer_t& factors) {

            factors.encode_text(text, *this);
        }

        inline void factor_length_range(Range) {
            // ignore (not needed)
        }
    };

    class Decoder {
    private:
        std::unique_ptr<io::InputStream> m_in;

    public:
        inline Decoder(const Config& cfg, Input& in) {
            m_in = std::make_unique<io::InputStream>(in.as_stream());
        }

        template<typename decomp_t>
        inline void decode(decomp_t& decomp) {
            throw std::runtime_error("not implemented");
        }
    };

    template<typename literals_t>
    inline auto encoder(Output& output, literals_t&& literals) {
        return Encoder(config(), output);
    }

    inline auto decoder(Input& input) {
        return Decoder(config(), input);
    }
};

}}
