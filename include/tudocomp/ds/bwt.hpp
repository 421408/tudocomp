#pragma once

#include <cstdint>
#include <tudocomp/util/View.hpp>
#include <tudocomp/util.hpp>
#include <tudocomp/def.hpp>

namespace tdc {

/// \brief Contains functionality for computing and decoding the Burrows-Wheeler
///        transform (BWT) of a text.
namespace bwt {
		static_assert(std::is_same<View::value_type, uliteral_t>::value, "View::value_type and uliteral_t must be the same");

/**
 * Computes the value BWT[i] of a text T given its suffix array SA
 * Runs in O(1) time since BWT[i] = SA[(T[i]-1) mod |SA|]
 */
template<typename text_t, typename sa_t>
inline typename text_t::value_type bwt(const text_t& text, const sa_t& sa, const size_t i) {
	return (sa[i] == 0u) ? text[sa.size()-1] : text[sa[i]-1];
}

/**
 * Computes the LF table used for decoding the BWT
 * Input is a BWT and its length
 */
template<typename bwt_t>
len_compact_t* compute_LF(const bwt_t& bwt, const size_t bwt_length) {
	DVLOG(2) << "Computing LF";
	if(bwt_length == 0) return nullptr;
	len_compact_t C[ULITERAL_MAX+1] { 0 }; // alphabet counter
	for(uliteral_t c : bwt) {
		++C[c];
	}
	DVLOG(2) << "histogram: " << arr_to_debug_string(C,ULITERAL_MAX+1);

    len_compact_t pred = 0;
	for(size_t i = 0; i <= ULITERAL_MAX; ++i) {
        auto occ_i = C[i];
        C[i] = pred;
        pred += occ_i;
	}

	DVLOG(2) << "C: " << arr_to_debug_string(C,ULITERAL_MAX+1);
	DCHECK_EQ(C[0],0u); // no character preceeds 0
	DCHECK_EQ(C[1],1u); // there is exactly only one '\0' byte
    DCHECK_EQ(pred, bwt_length); // C[sigma+1] = n

	len_compact_t* LF { new len_compact_t[bwt_length] };
	for(len_t i = 0; i < bwt_length; ++i) {
		LF[i] = C[bwt[i]];
		++C[bwt[i]];
	}

	DVLOG(2) << "LF: " << arr_to_debug_string(LF, bwt_length);

    IF_PARANOID(
	    DCHECK([&] () { // unique invariant of the LF mapping
			    assert_permutation(LF,bwt_length);
			    for(len_t i = 0; i < bwt_length; ++i)
			    for(len_t j = i+1; j < bwt_length; ++j) {
			    if(bwt[i] != bwt[j]) continue;
			    DCHECK_LT(LF[i], LF[j]);
			    }
			    return true;
			    }());
    )

	DVLOG(2) << "Finished Computing LF";
	return LF;
}


/**
 * Decodes a BWT
 * It is assumed that the BWT is stored in a container with access to operator[] and .size()
 */
template<typename bwt_t>
std::string decode_bwt(const bwt_t& bwt) {
	const size_t bwt_length = bwt.size();
	DVLOG(2) << "InputSize: " << bwt_length;
	if(tdc_unlikely(bwt_length <= 1)) return std::string();

	const len_compact_t*const LF { compute_LF(bwt, bwt_length) };

    std::string decoded_string(bwt_length-1, 0);
    //decoded_string[bwt_length-1] = 0;

	len_t i = 0;
	for(len_t j = 1; j < bwt_length; ++j) {
		decoded_string[bwt_length - j-1] = bwt[i];
		i = LF[i];

        // this debug line only holds for texts that are guaranteed not
        // to contain no ZERO bytes
		//DCHECK( (bwt[i] == 0 && j+1 == bwt_length) || (bwt[i] != 0 && j+1 < bwt_length));
	}
	delete [] LF;
	return decoded_string;
}

}}//ns

