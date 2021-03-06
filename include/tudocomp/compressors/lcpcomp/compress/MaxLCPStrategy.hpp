#pragma once

#include <tudocomp/Algorithm.hpp>
#include <tudocomp/ds/DSDef.hpp>

#include <tudocomp/compressors/lzss/FactorBuffer.hpp>
#include <tudocomp/compressors/lcpcomp/lcpcomp.hpp>
#include <tudocomp/compressors/lcpcomp/MaxLCPSuffixList.hpp>

#include <tudocomp_stat/StatPhase.hpp>

namespace tdc {
namespace lcpcomp {

/// Implements the original "Max LCP" selection strategy for LCPComp.
///
/// This strategy constructs a deque containing suffix array entries sorted
/// by their LCP length. The entry with the maximum LCP is selected and
/// overlapping suffices are removed from the deque.
///
/// This was the original naive approach in "Textkompression mithilfe von
/// Enhanced Suffix Arrays" (BA thesis, Patrick Dinklage, 2015).
class MaxLCPStrategy : public Algorithm {
public:
    inline static Meta meta() {
        Meta m(comp_strategy_type(), "max_lcp");
        return m;
    }

    template<typename ds_t>
    inline static void construct_textds(ds_t& ds) {
        ds.template construct<
            ds::SUFFIX_ARRAY,
            ds::LCP_ARRAY,
            ds::INVERSE_SUFFIX_ARRAY>();
    }

    using Algorithm::Algorithm; //import constructor

    template<typename text_t, typename factorbuffer_t>
    inline void factorize(text_t& text, size_t threshold, factorbuffer_t& factors) {
        // get data structures
        auto& sa = text.template get<ds::SUFFIX_ARRAY>();
        auto& isa = text.template get<ds::INVERSE_SUFFIX_ARRAY>();

        const size_t max_lcp = text.template get_provider<ds::LCP_ARRAY>().max_lcp;
        StatPhase::log("maxlcp", max_lcp);
        auto lcp = text.template relinquish<ds::LCP_ARRAY>();

        auto list = StatPhase::wrap("Construct MaxLCPSuffixList", [&]{
            MaxLCPSuffixList<decltype(lcp)> list(lcp, threshold, max_lcp);
            StatPhase::log("entries", list.size());
            return list;
        });

        //Factorize
        StatPhase::wrap("Process MaxLCPSuffixList", [&]{
            while(list.size() > 0) {
                //get suffix with longest LCP
                size_t m = list.get_max();

                //generate factor
                size_t fpos = sa[m];
                size_t fsrc = sa[m-1];
                size_t flen = lcp[m];

                factors.emplace_back(fpos, fsrc, flen);

                //remove overlapped entries
                for(size_t k = 0; k < flen; k++) {
                    size_t i = isa[fpos + k];
                    if(list.contains(i)) {
                        list.remove(i);
                    }
                }

                //correct intersecting entries
                for(size_t k = 0; k < flen && fpos > k; k++) {
                    size_t s = fpos - k - 1;
                    size_t i = isa[s];
                    if(list.contains(i)) {
                        if(s + lcp[i] > fpos) {
                            size_t l = fpos - s;
                            if(l >= threshold) {
                                list.decrease_key(i, l);
                            } else {
                                list.remove(i);
                            }
                        }
                    }
                }
            }

            StatPhase::log("num_factors", factors.size());
        });
    }
};

}}

