#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include <tudocomp/Compressor.hpp>
#include <tudocomp/Range.hpp>
#include <tudocomp/Tags.hpp>
#include <tudocomp/util.hpp>

#include <tudocomp/decompressors/LZSSDecompressor.hpp>

//custom

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <tuple>
#include <iostream>
#include <deque>

#include <tudocomp/util/bits.hpp>
#include <tudocomp/def.hpp>
#include <tudocomp/util/compact_hash/map/typedefs.hpp>

#include <tudocomp/compressors/lzss/FactorBuffer.hpp>
#include <tudocomp/compressors/lzss/UnreplacedLiterals.hpp>

#include <tudocomp/compressors/lz77Aprox/AproxFactor.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
#include <tudocomp/compressors/lz77Aprox/stackoverflow_hash.hpp>



namespace tdc
{
    template <typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor
    {
        //TYPEDEF
        typedef tdc::compact_hash::map::sparse_layered_hashmap_t<uint64_t> slhashmap64;

    private:
        
         //fills the given chain container with the right a mount of chains
        void populate_chainbuffer(std::deque<Chain> &curr_Chains,uint64_t &size,std::string &input_view){

            for(int i=0;i<input_view.size()-size*2;i=i+size){
                curr_Chains.push_back(Chain(64,i));
            }

        }

        void fill_hashmap(std::unordered_map<uint64_t, len_t> &hmap, std::deque<Chain> &curr_Chains, uint64_t &size, std::string &input_view, hash_interface *hash_provider)
        {
            hmap.reserve(curr_Chains.size());


            uint64_t hash;
            std::unordered_map<uint64_t,len_t>::iterator iter;

            for (int i = 0; i < curr_Chains.size(); i++)
            {
                    //increasing chain left
                    hash = hash_provider->make_hash(curr_Chains[i].get_position(), size, input_view);
                    iter = hmap.find(hash);

                    if(iter!=hmap.end()){
                        curr_Chains[i].add(size);
                    }
                    else{
                        hmap[hash]=i;
                    }

            }
        }

        void phase1_search(std::unordered_map<uint64_t, len_t> &hmap, std::deque<Chain> &curr_Chains, uint64_t &size, std::string &input_view, hash_interface *hash_provider)
        {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_t index;
            len_t rhash_pos=0;
            std::unordered_map<uint64_t, len_t>::iterator iter;

            for (int i = size; i < input_view.size(); i++)
            {
                rhash_pos=rhash.position;
                iter = hmap.find(rhash.hashvalue);
                if (iter != hmap.end())
                {
                    index = hmap[rhash.hashvalue];
                    if(rhash.position<curr_Chains[index].get_position()){
                        curr_Chains[index].add(size);
                    }
                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash,input_view);
            }
        }

        void new_chains(std::deque<Chain> &old_Chains, std::deque<Chain> &new_Chains, uint64_t &size,std::vector<Chain> phase2_buffer)
        {

            Chain left;
            bool curr_left;
            Chain right;
            bool curr_right;
            len_t position=0;

            while (!old_Chains.empty())
            {
                left = old_Chains[0];
                right = old_Chains[1];
                curr_left = left.get_chain() & size;
                curr_right = right.get_chain() & size;


                //only insert new chains if neither are found
                if (!(curr_right || curr_left))
                {
                    //old left
                    new_Chains.push_back(left);
                    //new right
                    new_Chains.push_back(Chain(64,left.get_position()+size/2));
                    //new left
                    new_Chains.push_back(Chain(64,right.get_position()));
                    //old right
                    right.set_position(right.get_position()+size/2);
                    new_Chains.push_back(right);

                    old_Chains.erase(old_Chains.begin(), old_Chains.begin() + 1);
                    continue;
                }
                if(curr_left&&curr_right)
                {
                    //cherry
                    //old left
                    left.set_position(left.get_position()+size-left.get_chain());
                    phase2_buffer.push_back(left);
                    //old right
                    phase2_buffer.push_back(right);

                    old_Chains.erase(old_Chains.begin(), old_Chains.begin() + 1);
                    continue;
                }
                if(curr_left){
                    //only left found
                    left.set_position(right.get_position());
                    new_Chains.push_back(left);
                    right.set_position(right.get_position()+size/2);
                    new_Chains.push_back(right);

                    old_Chains.erase(old_Chains.begin(), old_Chains.begin() + 1);
                    continue;
                }
                else{
                    //only rightfound
                    new_Chains.push_back(left);
                    right.set_position(right.get_position()+size/2);
                    new_Chains.push_back(right);

                    old_Chains.erase(old_Chains.begin(), old_Chains.begin() + 1);
                    continue;
                }

            }
        }

    public:
        inline static Meta meta()
        {
            Meta m(Compressor::type_desc(), "lz77Aprox",
                   "Computes a LZSS-parse based on binarytree-partioning "
                   "and merging of the resulting factors.");
            //do not remove you will get registery error
            //m.param must match template params and registery sub
            m.param("coder", "The output encoder.")
                .strategy<lzss_coder_t>(TypeDesc("lzss_coder"));
            m.param("window", "The starting window size").primitive(16);
            m.param("threshold", "The minimum factor length.").primitive(2);
            m.inherit_tag<lzss_coder_t>(tags::lossy);

            return m;
        }

        using Compressor::Compressor;

        inline virtual void compress(Input &input, Output &output) override
        {
            io::InputView input_view = input.as_view();
            auto os = output.as_stream();
            len_t WINDOW_SIZE = config().param("window").as_int() * 2;
            len_t MIN_FACTOR_LENGTH = config().param("threshold").as_int();

            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE)
            {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            //HASH
            hash_interface *hash_provider;
            hash_provider = new stackoverflow_hash();

            //bitsize of a chain
            len_t chain_size = log2(WINDOW_SIZE) - log2(MIN_FACTOR_LENGTH) + 1;
            //number of initianl chains
            len_t ini_chain_num = input_view.size() / WINDOW_SIZE;

            //make reusable containers
            //INIT: Phase 1
            std::deque<Chain> curr_Chains(ini_chain_num, Chain(64));
            std::deque<Chain> old_Chains;
            
            rolling_hash rhash;

            len_t level = 0;

            std::unordered_map<uint64_t, len_t> hmap;

            len_t size = WINDOW_SIZE;
            len_t round = 0;
            StatPhase::wrap("Phase1", [&] {
                while (size > MIN_FACTOR_LENGTH)
                {
                    StatPhase::wrap("Round: " + std::to_string(round), [&] {
                        StatPhase::wrap("make hashmap", [&] { fill_hashmap(hmap, curr_Chains, size, input_view, hash_provider); });
                        StatPhase::wrap("search", [&] { phase1_search(hmap, curr_Chains, size, input_view, hash_provider); });
                        StatPhase::wrap("move old", [&] { old_Chains = std::move(curr_Chains); });
                        curr_Chains.clear();
                        StatPhase::wrap("make new chains", [&] { new_chains(old_Chains, curr_Chains, size); });

                        size = size / 2;
                    });
                }
            });

            
            lzss::FactorBufferRAM factors;
            // encode
            StatPhase::wrap("Encode", [&] {
                auto coder = lzss_coder_t(config().sub_config("coder")).encoder(output, lzss::UnreplacedLiterals<decltype(input_view), decltype(factors)>(input_view, factors));

                coder.encode_text(input_view, factors);
            });
        }

        inline std::unique_ptr<Decompressor> decompressor() const override
        {
            return Algorithm::instance<LZSSDecompressor<lzss_coder_t>>();
        }
    };

} // namespace tdc
