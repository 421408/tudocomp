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
#include <bitset>

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
            for (len_compact_t i = 0; i < input_view.size() - 1 - size * 2; i = i + size * 2)
            {
                //std::cout<<"i: "<<i<<std::endl;
                curr_Chains.push_back(Chain(64, i));
                curr_Chains.push_back(Chain(64, i + size));
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
                if(hash==162439299){
                            std::cout << "here hmapmake pos: " << i << std::endl;

                        }


                //std::cout<<"0:4"<<std::endl;
                if (iter != hmap.end())
                {
                    if (curr_Chains[iter->second].get_position() < curr_Chains[i].get_position())
                    {
                        curr_Chains[i].add(size);
                        //std::cout<<"hash: "<<hash<<" pos: "<<input_view.substr(curr_Chains[i].get_position(),size)<<std::endl;
                        if(hash==162439299){
                            std::cout << "here hmapmake pos added " << i << std::endl;

                        }
                    }
                    else
                    {
                        curr_Chains[iter->second].add(size);
                        hmap[hash] = i;
                        if(hash==162439299){
                            std::cout << "here hmapmake pos else: " << i << std::endl;

                        }
                    }
                    // if(size>4096){
                    // std::cout<<"size to large chain: "<<curr_Chains[i].get_chain()<<" size: "<<size<<std::endl;
                    // throw std::invalid_argument( "chain to large group4" );
                    // }
                }
                else
                {
                    hmap[hash] = i;
                    //std::cout<<"inser hash: "<<hash<<" pos: "<<input_view.substr(curr_Chains[i].get_position(),size)<<std::endl;
                }
            }
            std::cout << "hashmapsize: " << hmap.size() << std::endl;
        }

        void phase1_search(std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<Chain> &curr_Chains, len_compact_t size, io::InputView &input_view, hash_interface *hash_provider, std::unordered_map<uint64_t, len_compact_t> &hmap_storage)
        {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_compact_t index;
            std::cout << "sizep1s: " << size << std::endl;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
            int last =0;

            

            for (len_compact_t i = size; i < input_view.size(); i++)
            {
                //std::cout<<" rhash: "<<rhash.hashvalue<<" string: "<<input_view.substr(rhash.position,size)<<" pos: "<<rhash.position<<std::endl;
                iter = hmap.find(rhash.hashvalue);
                last++;
                 

                if (iter != hmap.end())
                {
                    index = hmap[rhash.hashvalue];
                    if(rhash.position==273393){
                            std::cout << "here rpos hash: " << rhash.hashvalue << std::endl;

                        }
                    if (rhash.position < curr_Chains[index].get_position())
                    {
                        curr_Chains[index].add(size);

                        hmap_storage[rhash.hashvalue] = rhash.position;
                        
                        // std::cout << "rpos: " << rhash.position << std::endl;

                        // std::cout<<" found rhash : "<<rhash.position<<input_view.substr(rhash.position,size)<<std::endl;

                        // std::cout<<" found strpos: "<<curr_Chains[index].get_position()<<input_view.substr(curr_Chains[index].get_position(),size)<<std::endl;
                    }
                    if (rhash.position == curr_Chains[index].get_position())
                    {
                        
                        hmap_storage[rhash.hashvalue] = rhash.position;
                    }

                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash, input_view);
            }
            std::cout << "search done p1 last: " << last << std::endl;
        }

        void new_chains(std::vector<Chain> &Chains, len_compact_t size, std::vector<Chain> &phase2_buffer)
        {

            Chain left;
            bool curr_left;
            Chain right;
            bool curr_right;
            len_compact_t curr_size;

            len_compact_t oldsize = Chains.size();

            for (len_compact_t i = 0; i < oldsize && i < Chains.size(); i = i + 2)
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
                    //  std::cout << "split" << std::endl;
                    // std::cout << "______" << std::endl;
                    // for (Chain c : Chains)
                    // {
                    //     std::cout << "chain: " << std::bitset<18>(c.get_chain()) << " pos: " << c.get_position() << std::endl;
                    // }
                    continue;
                }
                if (curr_left && curr_right)
                {
                    //cherry

                    //old left
                   
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
                    //  std::cout << "cherry" << std::endl;
                    // std::cout << "______" << std::endl;
                    // for (Chain c : Chains)
                    // {
                    //     std::cout << "chain: " << std::bitset<18>(c.get_chain()) << " pos: " << c.get_position() << std::endl;
                    // }
                    continue;
                }
                if (curr_left)
                {
                    //only left found
                    Chains[i].set_position(right.get_position());

                    Chains[i + 1].set_position(right.get_position() + size / 2);
                    //  std::cout << "leftfound" << std::endl;
                    // std::cout << "______" << std::endl;
                    // for (Chain c : Chains)
                    // {
                    //     std::cout << "chain: " << std::bitset<18>(c.get_chain()) << " pos: " << c.get_position() << std::endl;
                    // }
                }
                else
                {
                    //only rightfound

                    Chains[i + 1].set_position(left.get_position() + size / 2);
                    //  std::cout << "rightfound" << std::endl;
                    // std::cout << "______" << std::endl;
                    // for (Chain c : Chains)
                    // {
                    //     std::cout << "chain: " << std::bitset<18>(c.get_chain()) << " pos: " << c.get_position() << std::endl;
                    // }
                }
            }
            Chains.shrink_to_fit();
        }

        inline void find_next_search_groups(std::vector<Group> &groupVec, std::vector<len_compact_t> &active_index)
        {
            uint16_t size = groupVec.front().get_next_length() * 2;
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
            std::cout << "activeindexsize: " << active_index.size() << std::endl;
            if (active_index.size() == 1)
            {
                std::cout << "__________" << std::endl;
                std::cout << "active_index[0]" << active_index[0] << " pos: " << groupVec[active_index[0]].get_position() << std::endl;
                uint16_t c = groupVec[active_index[0]].get_chain();
                std::cout << "group: " << std::bitset<16>(c) << std::endl;
                std::cout << "startofs: " << groupVec[active_index[0]].get_start_of_search() << std::endl;
            }
        }

        void fill_group_hashmap(std::vector<Group> &groupVec, std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<len_compact_t> &active_index, io::InputView &input_view, hash_interface *hash_provider)
        {

            uint16_t size = groupVec[active_index.front()].get_next_length() * 2;
            uint64_t hash;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;

            for (len_compact_t i : active_index)
            {
                len_t k = groupVec[i].get_start_of_search();
                //std::cout << "Groupstarts " << k << " pos " << groupVec[i].get_position()<<" src: "<<groupVec[i].get_src_position() << std::endl;
                hash = hash_provider->make_hash(k, size, input_view);

                iter = hmap.find(hash);

                if (iter != hmap.end())
                {
                    //NOt in order
                    len_t old = groupVec[iter->second].get_start_of_search();

                    if (k < old)
                    {
                        groupVec[iter->second].absorp_next(k);
                        hmap[hash] = i;
                    }
                    else
                    {
                        groupVec[i].absorp_next(old);
                    }
                }
                else
                {
                    hmap[hash] = i;
                }
            }
            std::cout << "hmap filled p2 size: " << hmap.size() << "  :size " << size << std::endl;
            if (size==64 )
            {
                std::cout << hmap.begin()->first << std::endl;
            }
        }

        inline void phase2_search(uint16_t size, std::vector<Group> &groupVec, std::unordered_map<uint64_t, len_compact_t> &hmap, std::vector<lz77Aprox::Factor> &factorVec, io::InputView &input_view, hash_interface *hash_provider)
        {
            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);

            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
            len_compact_t index;
            

            for (len_compact_t i = size - 1; i < input_view.size(); i++)
            {
               
                iter = hmap.find(rhash.hashvalue);
               
                if (iter != hmap.end())
                {
                    index = hmap[rhash.hashvalue];
                    if (rhash.position < groupVec[index].get_start_of_search())
                    {
                        groupVec[index].absorp_next(rhash.position);
                    }
                    else
                    {
                        //this works for all cause thr rolling hash runs over each one
                        factorVec.push_back(groupVec[index].advance_Group());
                    }
                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash, input_view);
            }

            if (!hmap.empty())
            {
                //this happens sometimes with the last factor if the searchterm reaches beyond the file
                factorVec.push_back(groupVec[hmap.begin()->second].advance_Group());
            }

            std::cout << "search done p2 last: " << std::endl;
        }

        inline void check_groups(uint16_t size, std::vector<Group> &groupVec, std::vector<len_compact_t> &active_index, std::vector<lz77Aprox::Factor> &factorVec)
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
                    factorVec.push_back(g.advance_Group());
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

        void cherrys_to_groups(std::vector<Group> &groupVec, std::vector<lz77Aprox::Factor> &factorVec, std::vector<Chain> &phase2_buffer, uint16_t threshold)
        {

            Chain c;
            // start with decreasing
            bool inc = false;
            while (!phase2_buffer.empty())
            {
                c = phase2_buffer.back();
                if (1 == __builtin_popcount(c.get_chain()))
                {
                    //chain has only one factor skip phase 2
                    if (inc)
                    {
                        factorVec.push_back(lz77Aprox::Factor(c.get_position() - c.get_chain() + 1, c.get_position() - c.get_chain() + 1, c.get_chain(), true));
                    }
                    else
                    {
                        factorVec.push_back(lz77Aprox::Factor(c.get_position(), c.get_position(), c.get_chain(), true));
                    }
                    inc = !inc;
                    phase2_buffer.pop_back();

                    continue;
                }
                if (c.get_chain())
                {
                    //std::cout << "counter: " << groupVec.size() << std::endl;
                    //chain is not null and not 1
                    uint16_t lc = 1 << __builtin_ctz(c.get_chain());
                    groupVec.push_back(Group(c, inc, lc));

                    inc = !inc;
                    phase2_buffer.pop_back();

                    continue;
                }
            }
            phase2_buffer.clear();
        }

        void chains_to_groups(std::vector<Group> &groupVec, std::vector<lz77Aprox::Factor> &factorVec, std::vector<Chain> &curr_Chains, uint16_t threshold)
        {

            bool inc = false;
            Chain c;
            for (len_t i = 0; i < curr_Chains.size(); i++)
            {
                c = curr_Chains[i];
                //std::cout << "c pos 2: " << c.get_position() << std::endl;
                if (1 == __builtin_popcount(c.get_chain()))
                {
                    //chain has only one factor skip phase 2
                    bool last_set = threshold & c.get_chain();
                    if (!last_set && inc)
                    {
                        factorVec.push_back(lz77Aprox::Factor(c.get_position() + threshold, c.get_position() + threshold, c.get_chain(), false));
                    }
                    if (!last_set && !inc)
                    {
                        factorVec.push_back(lz77Aprox::Factor(c.get_position() - c.get_chain(), c.get_position() - c.get_chain(), c.get_chain(), false));
                    }

                    inc = !inc;

                    continue;
                }
                if (c.get_chain())
                {
                    //std::cout << "counter2: " << groupVec.size() << std::endl;

                    //chain is not null
                    groupVec.push_back(Group(c, inc, threshold));

                    inc = !inc;

                    continue;
                }
                inc = !inc;
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
            len_compact_t WINDOW_SIZE = config().param("window").as_int() * 2;
            len_compact_t MIN_FACTOR_LENGTH = config().param("threshold").as_int();
            std::cout << "inputsize: " << input_view.size() << std::endl;
            std::cout << "WINDOW_SIZE: " << WINDOW_SIZE << std::endl;
            std::cout << "Thres: " << MIN_FACTOR_LENGTH << std::endl;

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

            std::vector<std::unordered_map<uint64_t, len_compact_t>> hmap_storage;

            std::unordered_map<uint64_t, len_compact_t> temp_hmap_storage;

            len_compact_t size = WINDOW_SIZE / 2;
            len_compact_t round = 0;
            //std::cout << "0" << std::endl;
            StatPhase::wrap("Phase1", [&] {
                StatPhase::wrap("populate", [&] { populate_chainbuffer(curr_Chains, size, input_view); });
                //std::cout << curr_Chains[0].get_position() << std::endl;
                while (size >= MIN_FACTOR_LENGTH)
                {

                    // for(Chain c: curr_Chains){
                    //     std::cout<<"chain: "<<std::bitset<16>(c.get_chain())<<std::endl;
                    // }

                    StatPhase::wrap("Round: " + std::to_string(round), [&] {
                        StatPhase::wrap("make hashmap", [&] { fill_chain_hashmap(hmap, curr_Chains, size, input_view, hash_provider); });
                        //std::cout << "1" << std::endl;
                        StatPhase::wrap("search", [&] { phase1_search(hmap, curr_Chains, size, input_view, hash_provider, temp_hmap_storage); });
                        //std::cout << "2" << std::endl;
                        //std::cout << "temp-hmap-storage-size: " << temp_hmap_storage.size() << std::endl;
                        hmap_storage.push_back(std::move(temp_hmap_storage));
                        //std::cout << "hmap-storage-size: " << hmap_storage.size() << std::endl;
                        temp_hmap_storage = std::unordered_map<uint64_t, len_compact_t>();
                        hmap = std::unordered_map<uint64_t, len_compact_t>();
                        //     for(Chain c: curr_Chains){
                        //     std::cout<<"chain: "<<std::bitset<18>(c.get_chain())<<std::endl;
                        // }

                        // on the last level this doesnt need to be done
                        if (size != MIN_FACTOR_LENGTH)
                        {
                            StatPhase::wrap("make new chains", [&] { new_chains(curr_Chains, size, phase2_buffer); });
                        }
                        //std::cout << "3" << std::endl;
                        size = size / 2;
                        round++;
                    });
                    // std::cout << "size: " << size << std::endl;
                    // for(Chain c: curr_Chains){
                    //     std::cout<<"chain: "<<std::bitset<18>(c.get_chain())<<std::endl;
                    // }
                }
            });
            //std::cout << "phase1 done;" << std::endl;
            std::vector<Group> groupVec;
            std::vector<lz77Aprox::Factor> factorVec;

            len_t counter = 0;
            StatPhase::wrap("transfer", [&] {
                //add all cherry in phase 2 buffer to groupvec
                cherrys_to_groups(groupVec, factorVec, phase2_buffer, MIN_FACTOR_LENGTH);

                chains_to_groups(groupVec, factorVec, curr_Chains, MIN_FACTOR_LENGTH);
                counter = 0;
                //start decrasing
            });
            //std::cout << "transver done.g size: " << groupVec.size() << std::endl;
            //std::cout << "facsize: " << factorVec.size() << std::endl;
            curr_Chains.clear();
            curr_Chains.shrink_to_fit();
            //std::cout << "curr_CHains ajust" << std::endl;
            //Phase 2
            std::vector<len_compact_t> active_index;
            StatPhase::wrap("Phase2", [&] {
                round = 0;
                while (!groupVec.empty())
                {

                    //std::cout << "____" << size << std::endl;
                    // for(Group g: groupVec){
                    //     std::cout<<"gchain: "<<std::bitset<18>(g.get_chain())<<" gpos: "<<g.get_position()<<" src: "<<g.get_src_position()<<std::endl;
                    // }

                    StatPhase::wrap("Round: " + std::to_string(round), [&] {
                        //std::cout << "0" << std::endl;
                        StatPhase::wrap("findGroup", [&] { find_next_search_groups(groupVec, active_index); });
                        //std::cout << "01" << std::endl;
                        StatPhase::wrap("fill hmap", [&] { fill_group_hashmap(groupVec, hmap, active_index, input_view, hash_provider); });
                        size = groupVec[active_index.front()].get_next_length() * 2;
                        //std::cout << "02" << std::endl;
                        StatPhase::wrap("search", [&] { phase2_search(size, groupVec, hmap, factorVec, input_view, hash_provider); });
                        hmap = std::unordered_map<uint64_t, len_compact_t>();
                        //std::cout << "03" << std::endl;
                        //std::cout << "____" << std::endl;
                        // for(Group g: groupVec){
                        //     std::cout<<"gchain: "<<std::bitset<18>(g.get_chain())<<" gpos: "<<g.get_position()<<" src: "<<g.get_src_position()<<std::endl;
                        // }
                        StatPhase::wrap("cheack", [&] { check_groups(size, groupVec, active_index, factorVec); });
                        //std::cout << "____" << std::endl;
                        // for(Group g: groupVec){
                        //     std::cout<<"gchain: "<<std::bitset<18>(g.get_chain())<<" gpos: "<<g.get_position()<<" src: "<<g.get_src_position()<<std::endl;
                        // }
                        active_index.clear();
                        groupVec.shrink_to_fit();
                        //std::cout << "04" << std::endl;
                    });
                    round++;
                    //std::cout << "size: " << size << std::endl;
                }
            });
            std::cout << "phase2 done;" << std::endl;
            std::sort(factorVec.begin(), factorVec.end());

            // for(lz77Aprox::Factor f:factorVec){
            //      std::cout << "(" << f.pos << " , "<< f.len <<")";
            // }
            //std::cout<< std::endl;

            lzss::FactorBufferRAM factors;
            // encode

            uint16_t threshold_ctz = __builtin_ctz(MIN_FACTOR_LENGTH);

            std::cout << "hmap-storage-size: " << hmap_storage.size() << std::endl;

            for (lz77Aprox::Factor f : factorVec)
            {
                if (f.pos != f.src)
                {
                    factors.push_back(lzss::Factor(f.pos, f.src, f.len));

                    if (f.pos < f.src)
                    {
                        std::cout << "fpos: " << f.pos << " fsrc: " << f.src << "f.len: " << f.len << std::endl;
                        throw std::invalid_argument("forawrdref:");
                    }
                }
                else
                {
                    uint16_t hmap_index = hmap_storage.size() - 1 - (__builtin_ctz(f.len) - threshold_ctz);
                    uint64_t hash = hash_provider->make_hash(f.pos, f.len, input_view);
                    len_compact_t src = hmap_storage[hmap_index][hash];

                    factors.push_back(lzss::Factor(f.pos, src, f.len));
                    
                    if (input_view.substr(f.pos, f.len) != input_view.substr(src, f.len))
                    {
                        std::cout << "hmap_index: " << hmap_index << std::endl;
                        std::cout << "faclen: " << f.len << std::endl;
                        std::cout << "srcpos: " << src << std::endl;
                        std::cout << "facpos: " << f.pos << std::endl;
                        std::cout << "fac: " << input_view.substr(f.pos, f.len) <<std::endl<<"hash: "<<hash_provider->make_hash(f.pos,f.len,input_view)<< std::endl;
                        std::cout << "src: " << input_view.substr(src, f.len) <<std::endl<<"hash: "<<hash_provider->make_hash(src,f.len,input_view)<< std::endl;
                        // uint64_t hashp = hash_provider->make_hash(f.pos+1, f.len, input_view);
                        // len_compact_t srcp = hmap_storage[hmap_index][hashp];
                        // std::cout << "facp: " << input_view.substr(f.pos+1, f.len) <<std::endl<<" hash: "<<hash_provider->make_hash(f.pos+1,f.len,input_view)<< std::endl;
                        // std::cout << "srcp: " << input_view.substr(srcp, f.len) <<std::endl<<" hash: "<<hash_provider->make_hash(srcp,f.len,input_view)<< std::endl;
                        // uint64_t hashm = hash_provider->make_hash(f.pos-1, f.len, input_view);
                        // len_compact_t srcm = hmap_storage[hmap_index][hashm];
                        // std::cout << "facm: " << input_view.substr(f.pos-1, f.len) <<std::endl<<" hash: "<<hash_provider->make_hash(f.pos-1,f.len,input_view)<< std::endl;
                        // std::cout << "srcm: " << input_view.substr(srcm, f.len) <<std::endl<<" hash: "<<hash_provider->make_hash(srcm,f.len,input_view)<< std::endl;
                        throw std::invalid_argument("false ref:");
                    }
                }
            }
            std::cout << "facs: " << factorVec.size() << std::endl;
            std::cout << "facfound: " << factors.size() << std::endl;

            for (lzss::Factor f : factors)
            {
                if (input_view.substr(f.pos, f.len) != input_view.substr(f.src, f.len))
                {
                    std::cout << "(" << f.pos << " , " << f.src << " , " << f.len << ")" << std::endl;
                    std::cout << "srcstring: " << input_view.substr(f.src, f.len) << std::endl;
                    std::cout << "orgstring: " << input_view.substr(f.pos, f.len) << std::endl;
                    throw std::invalid_argument("false ref:");
                }
            }
            std::cout << "______________" << std::endl;

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
