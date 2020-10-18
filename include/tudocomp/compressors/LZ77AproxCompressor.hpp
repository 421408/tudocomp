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

#include <tudocomp/compressors/lz77Aprox/Group.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <tudocomp/compressors/lz77Aprox/Factor.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
#include <tudocomp/compressors/lz77Aprox/stackoverflow_hash.hpp>

namespace tdc
{
    template <typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor
    {

    private:
        //fills the given chain container with the right a mount of chains
        void populate_chainbuffer(std::vector<Chain> &curr_Chains, len_compact_t size, io::InputView &input_view)
        {
            //std::cout<<"pop:"<<std::endl;
            for (len_compact_t i = 0; i < input_view.size() - size * 2; i = i + size)
            {
                //std::cout<<"i: "<<i<<std::endl;
                curr_Chains.push_back(Chain(64, i));
                //std::cout<<"back: "<<curr_Chains.back().get_position()<<std::endl;
            }
        }

        void fill_chain_hashmap(std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<Chain> &curr_Chains, len_compact_t size, io::InputView &input_view, hash_interface *hash_provider)
        {
            hmap.reserve(curr_Chains.size());

            uint64_t hash;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;

            //std::cout<<"0:1"<<std::endl;

            for (len_compact_t i = 0; i < curr_Chains.size(); i++)
            {
                //increasing chain left
                //std::cout<<"0:2"<<std::endl;
                //std::cout<<curr_Chains[i].get_position()<<std::endl;
                hash = hash_provider->make_hash(curr_Chains[i].get_position(), size, input_view);
                //std::cout<<"0:3"<<std::endl;
                iter = hmap.find(hash);
                //std::cout<<"0:4"<<std::endl;
                if (iter != hmap.end())
                {
                    if(curr_Chains[iter->second].get_position()<curr_Chains[i].get_position()){
                        curr_Chains[i].add(size);
                    }
                    else{
                        curr_Chains[iter->second].add(size);
                        hmap[hash]=i;
                    }
                    // if(size>4096){
                    // std::cout<<"size to large chain: "<<curr_Chains[i].get_chain()<<" size: "<<size<<std::endl;
                    // throw std::invalid_argument( "chain to large group4" );
                    // }
                }
                else
                {
                    hmap[hash] = i;
                }
            }
        }

        void phase1_search(std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<Chain> &curr_Chains, len_compact_t size, io::InputView &input_view, hash_interface *hash_provider)
        {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_compact_t index;
            std::cout<<"sizep1s: "<<size<<std::endl;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;

            for (len_compact_t i = size; i < input_view.size(); i++)
            {

                iter = hmap.find(rhash.hashvalue);
                if (iter != hmap.end())
                {
                    index = hmap[rhash.hashvalue];
                    if (rhash.position < curr_Chains[index].get_position())
                    {
                        curr_Chains[index].add(size);
                    //     if(curr_Chains[index].get_chain()>(4096*2)-1){
                    // std::cout<<"size to large chain: "<<curr_Chains[i].get_chain()<<" size: "<<size<<std::endl;
                    // throw std::invalid_argument( "chain to large group4" );
                    // }
                    }
                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash, input_view);
            }
        }

        void new_chains(std::vector<Chain> &Chains, len_compact_t size, std::vector<Chain> &phase2_buffer)
        {

            Chain left;
            bool curr_left;
            Chain right;
            bool curr_right;
            len_compact_t curr_size;

            len_compact_t oldsize = Chains.size();

            for (len_compact_t i = 0; i < oldsize  && i<Chains.size(); i = i + 2)
            {
                left = Chains[i];
                right = Chains[i + 1];
                curr_left = left.get_chain() & size;
                curr_right = right.get_chain() & size;

                //only insert new chains if neither are found
                if (!(curr_right || curr_left))
                {
                    //old left has nothing to do

                    //new right
                    Chains[i + 1] = Chain(64, left.get_position() + size / 2);
                    //new left
                    Chains.push_back(Chain(64, right.get_position()));
                    //old right
                    right.set_position(right.get_position() + size / 2);
                    Chains.push_back(right);

                    continue;
                }
                if (curr_left && curr_right)
                {
                    //cherry
                    //old left
                    left.set_position(left.get_position() + size - 1);
                    phase2_buffer.push_back(left);
                    //old right
                    phase2_buffer.push_back(right);

                    curr_size = Chains.size();

                    Chains[i + 1] = std::move(Chains.back());
                    Chains.pop_back();
                    Chains[i] = std::move(Chains.back());
                    Chains.pop_back();
                    if (curr_size <= oldsize)
                    {
                        i = i - 2;
                    }

                    continue;
                }
                if (curr_left)
                {
                    //only left found
                    Chains[i].set_position(right.get_position());

                    Chains[i + 1].set_position(right.get_position() + size / 2);
                }
                else
                {
                    //only rightfound

                    Chains[i + 1].set_position(left.get_position() + size / 2);
                }
            }
            Chains.shrink_to_fit();
        }

        inline void find_next_search_groups(std::vector<Group> &groupVec, std::vector<len_compact_t> &active_index)
        {
            uint16_t size = groupVec.front().get_next_length()*2;
            for (len_compact_t i = 0; i < groupVec.size(); i++)
            {
                if (groupVec[i].get_next_length() * 2 < size)
                {
                    size = groupVec[i].get_next_length() * 2;
                    active_index.clear();
                    active_index.push_back(i);
                    continue;
                }
                if (groupVec[i].get_next_length() * 2 == size)
                {
                    active_index.push_back(i);
                }
            }
        }

        void fill_group_hashmap(std::vector<Group> &groupVec, std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<len_compact_t> &active_index, io::InputView &input_view, hash_interface *hash_provider)
        {
 
            uint16_t size = groupVec[active_index.front()].get_next_length() * 2;
            uint64_t hash;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
             std::cout << "001" << std::endl;
            for (len_compact_t i : active_index)
            {
                hash = hash_provider->make_hash(groupVec[i].get_start_of_search(), size, input_view);
                iter = hmap.find(hash);

                if (iter != hmap.end())
                {
                    groupVec[i].absorp_next(groupVec[iter->second].get_start_of_search());
                }
                else
                {
                    hmap[hash] = i;
                }
            }
        }

        inline void phase2_search(uint16_t size, std::vector<Group> &groupVec, std::unordered_map<uint64_t, len_compact_t> &hmap,std::vector<lz77Aprox::Factor> &factorVec, io::InputView &input_view, hash_interface *hash_provider)
        {
            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);

            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
            len_compact_t index;

            for (len_compact_t i = size; i < input_view.size(); i++)
            {

                iter = hmap.find(rhash.hashvalue);
                if (iter != hmap.end())
                {
                    index = hmap[rhash.hashvalue];
                    if (rhash.position < groupVec[index].get_start_of_search())
                    {
                        groupVec[index].absorp_next(rhash.position);
                    }
                    else{
                        //this works for all cause thr rolling hash runs over each one
                        factorVec.push_back(groupVec[index].advance_Group());
                    }
                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash, input_view);
            }
        }

        inline void check_groups(uint16_t size,std::vector<Group> &groupVec, std::vector<len_compact_t> &active_index, std::vector<lz77Aprox::Factor> &factorVec)
        {
            // if(size==4096){
            //     std::cout<<"number groups:"<<groupVec.size()<<std::endl;
            // }
            std::vector<len_compact_t> mark_delete;
            for (len_compact_t index : active_index)
            {
                Group g = groupVec[index];
                if (!g.has_next())
                {
                   
                    mark_delete.push_back(index);
                }
            }
            //delete all marked
            len_compact_t index;
            while (!mark_delete.empty())
            {
                index = mark_delete.back();
                if (groupVec.size() - 1 == index)
                {
                    groupVec.pop_back();
                }
                else
                {
                    groupVec[index] = groupVec.back();
                    groupVec.pop_back();
                }
                mark_delete.pop_back();
            }
                //         if(size==4096){
                // std::cout<<"number groups:"<<groupVec.size()<<std::endl;
                // for(Group g:groupVec){
                //     std::cout<<"chain:"<<g.get_chain()<<std::endl;
                // }
            // }
        }

        void cherrys_to_groups(std::vector<lz77Aprox::Factor> &facVec,std::vector<Chain> &phase2_buffer){

            Chain c;
            // start with decreasing
            bool inc = false;
            while (!phase2_buffer.empty())
            {
                c = phase2_buffer.back();
                if (1 == __builtin_popcount(c.get_chain()))
                {
                    //chain has only one factor skip phase 2
                    factorVec.push_back(lz77Aprox::Factor(c.get_position(), c.get_position(), c.get_chain(),true));
                   
                    inc = !inc;
                    phase2_buffer.pop_back();
                    
                    continue;
                }
                if (c.get_chain())
                {
                    //std::cout<<"counter: "<<counter<<std::endl;
                    //chain is not null and not 1
                    groupVec.push_back(Group(c, inc));
                    
                    inc = !inc;
                    phase2_buffer.pop_back();
                   
                    continue;
                }
               
            }
            phase2_buffer.clear();
        }

        void chains_to_groups(std::vector<lz77Aprox::Factor> &facVec,std::vector<Chain> &groupVec){
           
            bool inc = false;
             Chain c;
            for(len_t i=0; i<curr_Chains.size();i++)
            {
                c = curr_Chains[i];

                if (1 == __builtin_popcount(c.get_chain()))
                {
                    //chain has only one factor skip phase 2
                    factorVec.push_back(lz77Aprox::Factor(c.get_position(), c.get_position(), c.get_chain(),false));
                   
                    inc = !inc;
                    
                    continue;
                }
                if (c.get_chain())
                {
                     //std::cout<<"counter2: "<<counter<<std::endl;
                    //chain is not null
                    groupVec.push_back(Group(c, inc));
                    
                    inc = !inc;
                   
                    continue;
                }
            }
            groupVec.clear();
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

            std::cout << "compact: " << sizeof(len_compact_t) << std::endl;
            std::cout << "len_t: " << sizeof(len_t) << std::endl;
            std::cout << "chain: " << sizeof(Chain) << std::endl;
            io::InputView input_view = input.as_view();
            auto os = output.as_stream();
            len_compact_t WINDOW_SIZE = config().param("window").as_int() * 2;
            len_compact_t MIN_FACTOR_LENGTH = config().param("threshold").as_int();

            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE)
            {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            //HASH
            hash_interface *hash_provider;
            hash_provider = new stackoverflow_hash();

            //bitsize of a chain
            //len_compact_t chain_size = log2(WINDOW_SIZE) - log2(MIN_FACTOR_LENGTH) + 1;
            //number of initianl chains
            //len_compact_t ini_chain_num = ((input_view.size()-1) / WINDOW_SIZE)*2;

            //make reusable containers
            //INIT: Phase 1
            std::vector<Chain> curr_Chains;

            std::vector<Chain> phase2_buffer;

            std::unordered_map<uint64_t, len_compact_t> hmap;

            len_compact_t size = WINDOW_SIZE / 2;
            len_compact_t round = 0;
            std::cout << "0" << std::endl;
            StatPhase::wrap("Phase1", [&] {
                StatPhase::wrap("populate", [&] { populate_chainbuffer(curr_Chains, size, input_view); });
                std::cout << curr_Chains[0].get_position() << std::endl;
                while (size > MIN_FACTOR_LENGTH)
                {
                    StatPhase::wrap("Round: " + std::to_string(round), [&] {
                        StatPhase::wrap("make hashmap", [&] { fill_chain_hashmap(hmap, curr_Chains, size, input_view, hash_provider); });
                        std::cout << "1" << std::endl;
                        StatPhase::wrap("search", [&] { phase1_search(hmap, curr_Chains, size, input_view, hash_provider); });
                        std::cout << "2" << std::endl;
                        hmap = std::unordered_map<uint64_t, len_compact_t>();
                        StatPhase::wrap("make new chains", [&] { new_chains(curr_Chains, size, phase2_buffer); });
                        std::cout << "3" << std::endl;
                        size = size / 2;
                        round++;
                    });
                    std::cout<<"size: "<<size<<std::endl;
                }
            });
            std::cout << "phase1 done;" << std::endl;
            std::vector<Group> groupVec;
            std::vector<lz77Aprox::Factor> factorVec;
            
             len_t counter=0;
            StatPhase::wrap("transfer", [&] {
            //add all cherry in phase 2 buffer to groupvec
            cherrys_to_groups(factorVec,phase2_buffer);
            
            
            std::cout << "transver half.counter: "<<counter << std::endl;

            chains_to_groups();
            counter=0;
            //start decrasing
            
            });
            std::cout << "transver done.counter: " <<counter<< std::endl;
            std::cout << "facsize: " <<factorVec.size()<< std::endl;
            curr_Chains.clear();
            curr_Chains.shrink_to_fit();
            //Phase 2
            std::vector<len_compact_t> active_index;
            StatPhase::wrap("Phase2", [&] {
            round = 0;
            while (!groupVec.empty())
            {
                StatPhase::wrap("Round: " + std::to_string(round), [&] {
                    std::cout << "0" << std::endl;
                    StatPhase::wrap("findGroup", [&] { find_next_search_groups(groupVec, active_index); });
                    std::cout << "01" << std::endl;
                    StatPhase::wrap("fill hmap", [&] { fill_group_hashmap(groupVec, hmap, active_index, input_view, hash_provider); });
                    size = groupVec[active_index.front()].get_next_length() * 2;
                    std::cout << "02" << std::endl;
                    StatPhase::wrap("search", [&] { phase2_search(size, groupVec, hmap,factorVec, input_view, hash_provider); });
                    hmap = std::unordered_map<uint64_t, len_compact_t>();
                    std::cout << "03" << std::endl;
                    StatPhase::wrap("cheack", [&] { check_groups(size,groupVec, active_index, factorVec); });
                    active_index.clear();
                    groupVec.shrink_to_fit();
                    std::cout << "04" << std::endl;
                });
                round++;
                std::cout<<"size: "<<size<<std::endl;
            }
            });

            std::sort(factorVec.begin(),factorVec.end());

            


            lzss::FactorBufferRAM factors;
            // encode

            for(lz77Aprox::Factor f :factorVec){
                if(f.pos!=f.src){
                    factors.push_back(lzss::Factor(f.pos,f.src,f.len));

                    if(f.pos<f.src){
                    std::cout<<"fpos: "<<f.pos<<" fsrc: "<<f.src<<"f.len: "<<f.len<<std::endl;
                       throw std::invalid_argument( "forawrdref:" );
                 }
                }
                    
            }
            std::cout<<"facs: "<<factorVec.size()<<std::endl;
            std::cout<<"facfound: "<<factors.size()<<std::endl;
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
