#pragma once

#include <tudocomp/util.hpp>
#include <tudocomp/io.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/Algorithm.hpp>

#include <tudocomp/ds/DSManager.hpp>
#include <tudocomp/ds/providers/DivSufSort.hpp>
#include <tudocomp/ds/providers/PhiAlgorithm.hpp>
#include <tudocomp/ds/providers/PhiFromSA.hpp>
#include <tudocomp/ds/providers/LCPFromPLCP.hpp>

#include <tudocomp_stat/StatPhase.hpp>

#include <vector>
#include <tuple>

namespace tdc {
namespace lfs {

template<typename ds_t = DSManager<DivSufSort, PhiFromSA, PhiAlgorithm, LCPFromPLCP>, uint min_lrf = 2>
class ESAStrategy : public Algorithm {
private:

    //(position in text, non_terminal_symbol_number, length_of_symbol);
    typedef std::tuple<uint,uint,uint> non_term;
    typedef std::vector<non_term> non_terminal_symbols;
    typedef std::vector<std::pair<uint,uint>> rules;

    BitVector dead_positions;

    inline virtual std::vector<uint> select_starting_positions(std::vector<uint> starting_positions, uint length){
        std::vector<uint> selected_starting_positions;
        std::sort(starting_positions.begin(), starting_positions.end());
        //select occurences greedily non-overlapping:
        selected_starting_positions.reserve(starting_positions.size());

        int last =  0-length;
        uint current;
        for (std::vector<uint>::iterator it=starting_positions.begin(); it!=starting_positions.end(); ++it){

            current = *it;
            //DLOG(INFO) << "checking starting position: " << current << " length: " << top.first << "last " << last;
            if(last+length <= current){
                selected_starting_positions.push_back(current);
                last = current;

            }

        }
        return selected_starting_positions;
    }



    //could be node_type
    std::vector<std::vector<uint> > lcp_bins;
public:

    using Algorithm::Algorithm; //import constructor

    inline static Meta meta() {
        Meta m(TypeDesc("lfs_comp"), "esa");
        m.param("ds", "The text data structure provider.")
            .strategy<ds_t>(ds::type(), Meta::Default<DSManager<DivSufSort, PhiFromSA, PhiAlgorithm, LCPFromPLCP>>());
        m.inherit_tag<ds_t>(tags::require_sentinel);
        return m;
    }


    inline void compute_rules(io::InputView & input, rules & dictionary, non_terminal_symbols & nts_symbols){
        // Construct text data structures
        ds_t ds(config().sub_config("ds"), input);
        StatPhase::wrap("computing sa and lcp", [&]{
            ds.template construct<ds::SUFFIX_ARRAY, ds::LCP_ARRAY>();
        });

        auto& sa_t = ds.template get<ds::SUFFIX_ARRAY>();
        auto& lcp_t = ds.template get<ds::LCP_ARRAY>();

        StatPhase::wrap("computing lrf occurences", [&]{

        // iterate over lcp array, add indexes with non overlapping prefix length greater than min_lrf_length to vector


        StatPhase::wrap("computing lrf occs", [&]{

        DLOG(INFO) << "iterate over lcp";
        uint dif ;
        uint factor_length;

        uint max = 0;

        for(uint i = 1; i<lcp_t.size(); i++){


            if(lcp_t[i] >= min_lrf){
                if(max < lcp_t[i]){
                    max = lcp_t[i];
                    lcp_bins.resize(max+1);
                }
                //compute length of non-overlapping factor:

                dif = std::abs((long)(sa_t[i-1] - sa_t[i]));
                factor_length = lcp_t[i];
                factor_length = std::min(factor_length, dif);

                //maybe greater non-overlapping factor possible with smaller suffix?
                int j =i-1;
                uint alt_dif;
                while(j>0 && lcp_t[j]>factor_length ){
                    alt_dif = std::abs((long)(sa_t[j] - sa_t[i]));
                    if(alt_dif>dif){
                        dif = alt_dif;
                    }
                    j--;


                }
                factor_length = lcp_t[i];
                factor_length = std::min(factor_length, dif);
                //second is position in suffix array
                //first is length of common prefix

                lcp_bins[factor_length].push_back(i);
            }
        }

        DLOG(INFO)<<"max lcp: "<<max;
        DLOG(INFO)<<"lcp bins: "<<lcp_bins.size();

        });

        if(lcp_bins.size()< min_lrf){
            DLOG(INFO)<<"nothing to replace, returning";
            return;
        }

        //Pq for the non-terminal symbols
        //the first in pair is position, the seconds the number of the non terminal symbol

        DLOG(INFO) << "computing lrfs";


        StatPhase::wrap("selecting occs", [&]{

            // Pop PQ, Select occurences of suffix, check if contains replaced symbols
        dead_positions = BitVector(input.size(), 0);

        nts_symbols.reserve(lcp_bins.size());
        uint non_terminal_symbol_number = 0;

        for(uint lcp_len = lcp_bins.size()-1; lcp_len>= min_lrf; lcp_len--){
            for(auto bin_it = lcp_bins[lcp_len].begin(); bin_it!=lcp_bins[lcp_len].end(); bin_it++){



                //detect all starting positions of this string using the sa and lcp:
                std::vector<uint> starting_positions;
                starting_positions.reserve(20);



                // and ceck in bitvector viable starting positions
                // there is no 1 bit on the corresponding positions
                // it suffices to check start and end position, because lrf can only be same length and shorter
                uint i = *bin_it;

                uint shorter_dif = lcp_len;


                while(i>=0 && ( lcp_t[i])>=lcp_len){
                    if(!dead_positions[sa_t[i-1]]  && !dead_positions[sa_t[i-1]+lcp_len-1]){
                        starting_positions.push_back(sa_t[i-1]);
                    }

                    if(!dead_positions[sa_t[i-1]] && dead_positions[sa_t[i-1]+lcp_len-1] ){
                        while(!dead_positions[sa_t[i-1]+lcp_len-shorter_dif]){
                            shorter_dif--;
                        }
                    }
                    i--;

                }
                i = *bin_it;
                while(i< lcp_t.size() &&  lcp_t[i]>=lcp_len){

                    if(!dead_positions[sa_t[i]]  && !dead_positions[sa_t[i]+lcp_len-1]){
                        starting_positions.push_back(sa_t[i]);
                    }
                    if(!dead_positions[sa_t[i]]   && dead_positions[sa_t[i]+lcp_len-1] ){
                        while(!dead_positions[sa_t[i]+lcp_len-shorter_dif]){
                            shorter_dif--;
                        }
                    }
                    i++;
                }
                if(starting_positions.size()>=2){
                    std::vector<uint> selected_starting_positions = select_starting_positions(starting_positions, lcp_len);
                    //computing substring to be replaced

                    if(selected_starting_positions.size()>=2){



                        uint offset = sa_t[*bin_it];
                        std::pair<uint,uint> longest_repeating_factor(offset, lcp_len);
                        for (std::vector<uint>::iterator it=selected_starting_positions.begin(); it!=selected_starting_positions.end(); ++it){
                            for(uint k = 0; k<lcp_len; k++){
                                dead_positions[*it+k]=1;
                            }

                            uint length_of_symbol = lcp_len;
                            std::tuple<uint,uint,uint> symbol(*it, non_terminal_symbol_number, length_of_symbol);
                            nts_symbols.push_back(symbol);
                        }

                        dictionary.push_back(longest_repeating_factor);
                        non_terminal_symbol_number++;
                    }
                }

            }
        }
        });
        });

        StatPhase::wrap("sorting symbols", [&]{
        DLOG(INFO) << "sorting symbols";
        std::sort(nts_symbols.begin(), nts_symbols.end());

        });

    }
};
}
}
