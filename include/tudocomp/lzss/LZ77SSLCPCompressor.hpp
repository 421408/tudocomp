#ifndef _INCLUDED_LZ77SS_LCP_COMPRESSOR_HPP
#define _INCLUDED_LZ77SS_LCP_COMPRESSOR_HPP

#include <algorithm>
#include <functional>
#include <vector>

#include <sdsl/int_vector.hpp>
#include <sdsl/suffix_arrays.hpp>
#include <sdsl/lcp.hpp>

#include <tudocomp/sdslex/int_vector_wrapper.hpp>
#include <tudocomp/util.h>

#include <tudocomp/lzss/LZSSCompressor.hpp>

namespace tudocomp {
namespace lzss {

/// Computes the LZ77 factorization of the input using its suffix array and
/// LCP table.
template<typename C>
class LZ77SSLCPCompressor : public LZSSCompressor<C> {

public:
    /// Default constructor (not supported).
    inline LZ77SSLCPCompressor() = delete;

    /// Construct the class with an environment.
    inline LZ77SSLCPCompressor(Env& env) : LZSSCompressor<C>(env) {
    }
    
protected:
    /// \copydoc
    inline virtual bool pre_factorize(Input& input) override {
        return false;
    }
    
    /// \copydoc
    inline virtual LZSSCoderOpts coder_opts(Input& input) override {
        return LZSSCoderOpts(true, bitsFor(input.size()));
    }
    
    /// \copydoc
    inline virtual void factorize(Input& input) override {
        auto in = input.as_view();

        size_t len = in.size();
        const uint8_t* in_ptr = in.mem_ptr();
        sdslex::int_vector_wrapper wrapper(in_ptr, len);

        //Construct SA
        sdsl::csa_bitcompressed<> sa;
        sdsl::construct_im(sa, wrapper.int_vector);

        //Construct ISA and LCP
        //TODO SDSL ???
        sdsl::int_vector<> isa(sa.size());
        sdsl::int_vector<> lcp(sa.size());

        for(size_t i = 0; i < sa.size(); i++) {
            isa[sa[i]] = i;

            if(i >= 2) {
                size_t j = sa[i], k = sa[i-1];
                while(in_ptr[j++] == in_ptr[k++]) ++lcp[i];
            }
        }

        sdsl::util::bit_compress(isa);
        sdsl::util::bit_compress(lcp);

        //Factorize
        size_t fact_min = 3; //factor threshold
        
        for(size_t i = 0; i < len;) {
            //get SA position for suffix i
            size_t h = isa[i];

            //search "upwards" in LCP array
            //include current, exclude last
            size_t p1 = lcp[h];
            ssize_t h1 = h - 1;
            if (p1 > 0) {
                while (h1 >= 0 && sa[h1] > sa[h]) {
                    p1 = std::min(p1, size_t(lcp[h1--]));
                }
            }

            //search "downwards" in LCP array
            //exclude current, include last
            size_t p2 = 0;
            size_t h2 = h + 1;
            if (h2 < len) {
                p2 = SSIZE_MAX;
                do {
                    p2 = std::min(p2, size_t(lcp[h2]));
                    if (sa[h2] < sa[h]) {
                        break;
                    }
                } while (++h2 < len);

                if (h2 >= len) {
                    p2 = 0;
                }
            }

            //select maximum
            size_t p = std::max(p1, p2);
            if (p >= fact_min) {
                LZSSCompressor<C>::handle_fact(LZSSFactor(i, sa[p == p1 ? h1 : h2], p));
                i += p; //advance
            } else {
                LZSSCompressor<C>::handle_sym(in_ptr[i]);
                ++i; //advance
            }
        }
    }
};

}}

#endif
