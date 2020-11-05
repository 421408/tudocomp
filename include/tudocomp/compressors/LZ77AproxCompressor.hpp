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
//#include <tudocomp/compressors/lz77Aprox/stackoverflow_hash.hpp>
//#include <tudocomp/compressors/lz77Aprox/berenstein_hash.hpp>
#include <tudocomp/compressors/lz77Aprox/dna_nth_hash.hpp>
//#include <tudocomp/compressors/lz77Aprox/buz_hash.hpp>

namespace tdc {
    template<typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor {

    private:
        //fills the given chain container with the right a mount of chains
        inline void
        populate_chainbuffer(std::vector <Chain> &curr_Chains, len_compact_t size, io::InputView &input_view) {

            for (len_compact_t i = 0; i < input_view.size() - 1 - size * 2; i = i + size * 2) {

                curr_Chains.push_back(Chain(64, i));
                curr_Chains.push_back(Chain(64, i + size));

            }
        }


        inline void fill_chain_hashmap(std::unordered_map <uint64_t, len_compact_t> &hmap,
                                       std::unordered_map <uint64_t, len_compact_t> &hmap_storage,
                                       std::vector <Chain> &curr_Chains, len_compact_t size, io::InputView &input_view,
                                       hash_interface *hash_provider, bool &collisions) {
            hmap.reserve(curr_Chains.size());


            uint64_t hash;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;


            for (len_compact_t i = 0; i < curr_Chains.size(); i++) {

                hash = hash_provider->make_hash(curr_Chains[i].get_position(), size, input_view);

                iter = hmap.find(hash);


                if (iter == hmap.end()) {
                    hmap[hash] = i;
                } else {
                    //NOT EMPTY
                    //Possible Collision
                    if (input_view.substr(curr_Chains[iter->second].get_position(), size) ==
                        input_view.substr(curr_Chains[i].get_position(), size)) {
                        //no collision
                        if (curr_Chains[iter->second].get_position() < curr_Chains[i].get_position()) {
                            //old lies before
                            curr_Chains[i].add(size);
                            insert_into_storage(hmap_storage, hash, curr_Chains[iter->second].get_position(), size,
                                                input_view);


                        } else {
                            //old lies behind
                            curr_Chains[iter->second].add(size);
                            hmap[hash] = i;
                            insert_into_storage(hmap_storage, hash, curr_Chains[i].get_position(), size, input_view);

                        }
                    } else {
                        //COLLISION
                        collisions = true;
                        bool inserted = false;
                        len_t offset = 1;
                        while (!inserted) {
                            iter = hmap.find(hash + offset);
                            if (iter == hmap.end()) {
                                //this bucket is empty
                                hmap[hash + offset] = i;
                                inserted = true;

                            } else {
                                //next buvket is not empty
                                if (input_view.substr(curr_Chains[iter->second].get_position(), size) ==
                                    input_view.substr(curr_Chains[i].get_position(), size)) {
                                    //strings match
                                    //no collision
                                    if (curr_Chains[iter->second].get_position() < curr_Chains[i].get_position()) {
                                        //old lies before
                                        curr_Chains[i].add(size);
                                        insert_into_storage(hmap_storage, hash,
                                                            curr_Chains[iter->second].get_position(), size, input_view);

                                    } else {
                                        //old lies behind
                                        curr_Chains[iter->second].add(size);
                                        hmap[hash + offset] = i;
                                        insert_into_storage(hmap_storage, hash, curr_Chains[i].get_position(), size,
                                                            input_view);

                                    }
                                    inserted = true;

                                }

                            }
                            offset++;
                        }
                    }
                }

            }

        }

        inline void insert_into_storage(std::unordered_map <uint64_t, len_compact_t> &hmap, int64_t hash, len_t pos,
                                        len_compact_t size, io::InputView &input_view) {
            bool inserted = false;
            len_t offset = 0;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;

            while (!inserted) {
                iter = hmap.find(hash + offset);
                if (iter == hmap.end()) {
                    hmap[hash + offset] = pos;
                    inserted = true;
                } else {
                    if (input_view.substr(iter->second, size) == input_view.substr(pos, size)) {
                        if (iter->second > pos) {
                            hmap[hash + offset] = pos;
                        }
                        inserted = true;
                    }
                }
                offset++;
            }
        }

        inline void phase1_search(std::unordered_map <uint64_t, len_compact_t> &hmap,
                                  std::vector <Chain> &curr_Chains,
                                  len_compact_t size, io::InputView &input_view, hash_interface *hash_provider,
                                  std::unordered_map <uint64_t, len_compact_t> &hmap_storage, bool &coll) {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_compact_t index;

            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
            int last = 0;
            len_t collisions = 0;
            if (coll) {
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    last++;

                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        if (input_view.substr(curr_Chains[index].get_position(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //NO COLLISION

                            if (rhash.position < curr_Chains[index].get_position()) {
                                curr_Chains[index].add(size);
                                insert_into_storage(hmap_storage, rhash.hashvalue, rhash.position, size, input_view);
                            }
                            if (hmap.find(rhash.hashvalue + 1) != hmap.end()) {
                                len_t offset = 1;
                                len_t dex = hmap[rhash.hashvalue + offset];
                                len_t ss = curr_Chains[dex].positon;
                                if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                    bool empty_found = false;
                                    while (!empty_found) {
                                        if (hmap.find(rhash.hashvalue + offset) != hmap.end()) {
                                            //check if hash match
                                            dex = hmap[rhash.hashvalue + offset];
                                            ss = curr_Chains[dex].positon;
                                            if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                                hmap[rhash.hashvalue + offset - 1] = dex;
                                                hmap.erase(rhash.hashvalue + offset);
                                            }
                                        } else {
                                            empty_found = true;
                                        }
                                        offset++;
                                    }
                                } else {
                                    hmap.erase(rhash.hashvalue);
                                }
                            } else {
                                hmap.erase(rhash.hashvalue);
                            }


                        } else {
                            //COLLISION
                            collisions++;
                            len_t offset = 1;
                            bool mt_or_right_found = false;
                            while (!mt_or_right_found) {
                                iter = hmap.find(rhash.hashvalue + offset);
                                if (iter != hmap.end()) {
                                    if (input_view.substr(curr_Chains[index].get_position(), size) ==
                                        input_view.substr(rhash.position, size)) {
                                        //STRING MATCHES
                                        if (rhash.position < curr_Chains[index].get_position()) {
                                            curr_Chains[index].add(size);

                                        }

                                        //do this to have src for duplicates in hmapfill
                                        insert_into_storage(hmap_storage, rhash.hashvalue + offset, rhash.position,
                                                            size, input_view);
                                        hmap.erase(rhash.hashvalue + offset);
                                        mt_or_right_found = true;

                                    }
                                } else {
                                    //empty bucket
                                    mt_or_right_found = true;
                                }
                                offset++;
                            }
                        }
                    }

                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            } else {
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        if (input_view.substr(curr_Chains[index].get_position(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //TRUE MATCH
                            if (rhash.position < curr_Chains[index].get_position()) {
                                curr_Chains[index].add(size);
                                insert_into_storage(hmap_storage, rhash.hashvalue, rhash.position, size, input_view);
                            }
                            hmap.erase(rhash.hashvalue);
                        }
                    }


                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            }


        }

        inline void new_chains(std::vector <Chain> &Chains, len_compact_t size, std::vector <Chain> &phase2_buffer) {

            Chain left;
            bool curr_left;
            Chain right;
            bool curr_right;
            len_compact_t curr_size;

            len_compact_t oldsize = Chains.size();

            for (len_compact_t i = 0; i < oldsize && i < Chains.size(); i = i + 2) {

                left = Chains[i];
                right = Chains[i + 1];
                curr_left = left.get_chain() & size;
                curr_right = right.get_chain() & size;

                //only insert new chains if neither are found
                if (!(curr_right || curr_left)) {
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
                if (curr_left && curr_right) {
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
                    if (curr_size <= oldsize) {
                        i = i - 2;
                        oldsize = oldsize - 2;
                    }

                    continue;
                }
                if (curr_left) {
                    //only left found
                    Chains[i].set_position(right.get_position());

                    Chains[i + 1].set_position(right.get_position() + size / 2);

                } else {
                    //only rightfound

                    Chains[i + 1].set_position(left.get_position() + size / 2);

                }
            }
            Chains.shrink_to_fit();
        }

        inline void find_next_search_groups(std::vector <Group> &groupVec, std::vector <len_compact_t> &active_index) {
            uint16_t size = groupVec.front().get_next_length() * 2;

            for (len_compact_t i = 0; i < groupVec.size(); i++) {


                if (groupVec[i].get_next_length() * 2 < size) {
                    size = groupVec[i].get_next_length() * 2;

                    active_index.clear();
                    active_index.push_back(i);
                    continue;
                }
                if (groupVec[i].get_next_length() * 2 == size) {
                    active_index.push_back(i);
                }
            }


        }

        inline void fill_group_hashmap(std::vector <Group> &groupVec,
                                       std::unordered_map <uint64_t, len_compact_t> &hmap,
                                       std::vector <lz77Aprox::Factor> &factorVec,
                                       std::vector <len_compact_t> &active_index, io::InputView &input_view,
                                       hash_interface *hash_provider, bool &collisions) {

            uint16_t size = groupVec[active_index.front()].get_next_length() * 2;
            uint64_t hash;
            std::unordered_map<uint64_t, len_compact_t>::iterator iter;


            for (len_compact_t i : active_index) {
                len_t k = groupVec[i].get_start_of_search();

                if (k + size < input_view.size()) {
                    hash = hash_provider->make_hash(k, size, input_view);

                    iter = hmap.find(hash);

                    if (iter == hmap.end()) {
                        hmap[hash] = i;

                    } else {
                        //POSSIBLE COLLISION

                        if (input_view.substr(groupVec[i].get_start_of_search(), size) ==
                            input_view.substr(groupVec[iter->second].get_start_of_search(), size)) {
                            //NO COLLISION
                            len_t old = groupVec[iter->second].get_start_of_search();

                            if (k < old) {

                                groupVec[iter->second].absorp_next(k);
                                hmap[hash] = i;

                            } else {
                                groupVec[i].absorp_next(old);

                            }
                        } else {
                            //COLLISION
                            bool inserted = false;
                            len_t offset = 1;
                            collisions = true;
                            while (!inserted) {
                                iter = hmap.find(hash + offset);
                                if (iter == hmap.end()) {
                                    //this bucket is empty
                                    hmap[hash + offset] = i;
                                    inserted = true;

                                } else {
                                    //next bucket is not empty
                                    if (input_view.substr(groupVec[iter->second].get_start_of_search(), size) ==
                                        input_view.substr(groupVec[i].get_start_of_search(), size)) {
                                        //strings match
                                        //no collision
                                        if (groupVec[i].get_start_of_search() <
                                            groupVec[iter->second].get_start_of_search()) {

                                            groupVec[iter->second].absorp_next(groupVec[i].get_start_of_search());
                                            hmap[hash + offset] = i;
                                        } else {
                                            groupVec[i].absorp_next(groupVec[iter->second].get_start_of_search());
                                        }
                                        inserted = true;

                                    }

                                }
                                offset++;
                            }


                        }
                    }
                } else {
                    //reaches beyond
                    factorVec.push_back(groupVec[i].advance_Group());

                }

            }

        }

        inline void
        phase2_search(uint16_t size, std::vector <Group> &groupVec, std::unordered_map <uint64_t, len_compact_t> &hmap,

                      std::vector <lz77Aprox::Factor> &factorVec, io::InputView &input_view,
                      hash_interface *hash_provider, bool &coll) {
            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);

            std::unordered_map<uint64_t, len_compact_t>::iterator iter;
            len_compact_t index;

            if (coll) {
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);


                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        if (input_view.substr(groupVec[index].get_start_of_search(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //NO COLLISSION
                            if (rhash.position < groupVec[index].get_start_of_search()) {
                                groupVec[index].absorp_next(rhash.position);

                            } else {
                                //this works for all cause thr rolling hash runs over each one
                                factorVec.push_back(groupVec[index].advance_Group());
                            }
                            if (hmap.find(rhash.hashvalue + 1) != hmap.end()) {
                                len_t offset = 1;
                                len_t dex = hmap[rhash.hashvalue + offset];
                                len_t ss = groupVec[dex].get_start_of_search();
                                if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {

                                    bool empty_found = false;
                                    while (!empty_found) {
                                        if (hmap.find(rhash.hashvalue + offset) != hmap.end()) {
                                            //check if hash match
                                            dex = hmap[rhash.hashvalue + offset];
                                            ss = groupVec[dex].get_start_of_search();
                                            if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                                hmap[rhash.hashvalue + offset - 1] = dex;
                                                hmap.erase(rhash.hashvalue + offset);
                                            }

                                        } else {
                                            empty_found = true;
                                        }
                                        offset++;
                                    }
                                } else {
                                    hmap.erase(rhash.hashvalue);
                                }

                            } else {
                                hmap.erase(rhash.hashvalue);
                            }

                            //REMEMBER THAT THE TRUE HASH IS THE LAST WITH COLLISION

                        } else {
                            //COLLISION
                            len_t offset = 1;
                            bool mt_or_right_found = false;
                            while (!mt_or_right_found) {
                                iter = hmap.find(rhash.hashvalue + offset);
                                if (iter != hmap.end()) {
                                    index = iter->second;
                                    if (input_view.substr(groupVec[index].get_position(), size) ==
                                        input_view.substr(rhash.position, size)) {
                                        //STRING MATCHES
                                        if (rhash.position < groupVec[index].get_start_of_search()) {
                                            groupVec[index].absorp_next(rhash.position);
                                        } else {
                                            //this works for all cause thr rolling hash runs over each one
                                            factorVec.push_back(groupVec[index].advance_Group());
                                        }

                                        hmap.erase(rhash.hashvalue + offset);
                                        mt_or_right_found = true;
                                    }
                                } else {
                                    //empty bucket
                                    mt_or_right_found = true;
                                }
                                offset++;
                            }

                        }

                    }

                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            } else {
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        if (input_view.substr(groupVec[index].get_start_of_search(), size) ==
                            input_view.substr(rhash.position, size)) {
                            if (rhash.position < groupVec[index].get_start_of_search()) {
                                groupVec[index].absorp_next(rhash.position);
                            } else {
                                //this works for all cause thr rolling hash runs over each one
                                factorVec.push_back(groupVec[index].advance_Group());
                            }
                            hmap.erase(rhash.hashvalue);
                        }
                    }
                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            }


            if (!hmap.empty()) {

                //this happens sometimes with the last factor if the searchterm reaches beyond the file
                // or collisions arnt hit
                for (auto iter : hmap) {
                    factorVec.push_back(groupVec[iter.second].advance_Group());

                }


            }


        }

        inline void
        check_groups(uint16_t size, std::vector <Group> &groupVec, std::vector <len_compact_t> &active_index,
                     std::vector <lz77Aprox::Factor> &factorVec) {


            std::vector <len_compact_t> mark_delete;
            for (len_compact_t index : active_index) {

                Group g = groupVec[index];
                if (!g.has_next()) {

                    mark_delete.push_back(index);
                    factorVec.push_back(g.advance_Group());
                }
            }
            //delete all marked


            len_compact_t index;
            while (!mark_delete.empty()) {
                index = mark_delete.back();

                if (groupVec.size() - 1 == index) {
                    groupVec.pop_back();
                } else {
                    groupVec[index] = groupVec.back();
                    groupVec.pop_back();
                }
                mark_delete.pop_back();
            }

        }

        inline void cherrys_to_groups(std::vector <Group> &groupVec, std::vector <lz77Aprox::Factor> &factorVec,
                                      std::vector <Chain> &phase2_buffer, uint16_t threshold) {

            Chain c;
            // start with decreasing
            bool inc = true;
            while (!phase2_buffer.empty()) {
                c = phase2_buffer.back();
                if (1 == __builtin_popcount(c.get_chain())) {
                    //chain has only one factor skip phase 2
                    if (inc) {
                        factorVec.push_back(
                                lz77Aprox::Factor(c.get_position(), c.get_position(), c.get_chain()));
                    } else {
                        factorVec.push_back(
                                lz77Aprox::Factor(c.get_position(), c.get_position(), c.get_chain()));
                    }
                    inc = !inc;
                    phase2_buffer.pop_back();

                    continue;
                }
                if (c.get_chain()) {

                    //chain is not null and not 1
                    uint16_t lc = 1 << __builtin_ctz(c.get_chain());
                    groupVec.push_back(Group(c, inc, lc));

                    inc = !inc;
                    phase2_buffer.pop_back();

                    continue;
                }
            }
            phase2_buffer.shrink_to_fit();
        }

        inline void chains_to_groups(std::vector <Group> &groupVec, std::vector <lz77Aprox::Factor> &factorVec,
                                     std::vector <Chain> &curr_Chains, uint16_t threshold) {

            bool inc = true;
            Chain c;
            len_t shrink_counter = 0;
            while (!curr_Chains.empty()) {
                if (shrink_counter == 4096 * 2) {
                    curr_Chains.shrink_to_fit();
                    shrink_counter = 0;
                }
                c = curr_Chains.back();
                shrink_counter++;
                if (1 == __builtin_popcount(c.get_chain())) {
                    //chain has only one factor skip phase 2
                    bool last_set = threshold & c.get_chain();
                    if (!last_set && inc) {
                        factorVec.emplace_back(
                                c.get_position() + threshold, c.get_position() + threshold,
                                std::move(c.get_chain()));
                    }
                    if (!last_set && !inc) {
                        factorVec.emplace_back(
                                c.get_position() - c.get_chain(), c.get_position() - c.get_chain(),
                                std::move(c.get_chain()));
                    }
                    if (last_set) {
                        factorVec.emplace_back(
                                c.get_position(), c.get_position(), std::move(c.get_chain()));
                    }

                    inc = !inc;
                    curr_Chains.pop_back();
                    continue;
                    if (c.get_chain()) {
                        //chain is not null
                        groupVec.push_back(Group(std::move(c), inc, threshold));

                        inc = !inc;
                        curr_Chains.pop_back();
                        continue;
                    }
                    inc = !inc;
                    curr_Chains.pop_back();
                }
                curr_Chains.shrink_to_fit();
            }
        }

    public:
        inline static Meta meta() {
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

        inline virtual void compress(Input &input, Output &output) override {

            io::InputView input_view = input.as_view();
            auto os = output.as_stream();
            len_compact_t WINDOW_SIZE = config().param("window").as_int() * 2;
            len_compact_t MIN_FACTOR_LENGTH = config().param("threshold").as_int();



            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE * 2) {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            //HASH
            hash_interface *hash_provider;

            //hash_provider = new berenstein_hash();
            hash_provider = new dna_nth_hash();
            //hash_provider = new stackoverflow_hash();
            //hash_provider = new buz_hash();


            //make reusable containers
            //INIT: Phase 1
            std::vector <Chain> curr_Chains;

            std::vector <Chain> phase2_buffer;

            std::unordered_map <uint64_t, len_compact_t> hmap;

            std::vector <std::unordered_map<uint64_t, len_compact_t>> hmap_storage;


            std::unordered_map <uint64_t, len_compact_t> temp_hmap_storage;


            len_compact_t size = WINDOW_SIZE / 2;
            len_compact_t round = 0;
            bool collision = false;


            populate_chainbuffer(curr_Chains, size, input_view);
            StatPhase::wrap("Phase1", [&] {
                StatPhase::log("Initial chain count", curr_Chains.size());
                while (size >= MIN_FACTOR_LENGTH) {
                    StatPhase::wrap("Round: " + std::to_string(round), [&] {

                        StatPhase::wrap("fill hashmap", [&] {
                            fill_chain_hashmap(hmap, temp_hmap_storage, curr_Chains, size, input_view, hash_provider,
                                               collision);
                        });

                        StatPhase::log("collisions", collision);
                        StatPhase::log("hmap.size", hmap.size());
                        StatPhase::wrap("Search", [&] {
                            phase1_search(hmap, curr_Chains, size, input_view, hash_provider,
                                          temp_hmap_storage, collision);
                        });


                        hmap_storage.push_back(std::move(temp_hmap_storage));


                        temp_hmap_storage = std::unordered_map<uint64_t, len_compact_t>();
                        hmap = std::unordered_map<uint64_t, len_compact_t>();



                        // on the last level this doesnt need to be done
                        if (size != MIN_FACTOR_LENGTH) {
                            StatPhase::wrap("make new chains", [&] { new_chains(curr_Chains, size, phase2_buffer); });
                        }
                        StatPhase::log("chain count", curr_Chains.size());
                        size = size / 2;
                        round++;
                        collision = false;
                    });

                }
            });


            std::vector <Group> groupVec;
            std::vector <lz77Aprox::Factor> factorVec;

            len_t counter = 0;
            StatPhase::wrap("transfer1", [&] {
                StatPhase::log("cherry count", phase2_buffer.size());
                //add all cherry in phase 2 buffer to groupvec
                cherrys_to_groups(groupVec, factorVec, phase2_buffer, MIN_FACTOR_LENGTH);
            });
            StatPhase::log("chain group count", groupVec.size());
            StatPhase::log("chain factor count", factorVec.size());
            StatPhase::wrap("transfer2", [&] {
                chains_to_groups(groupVec, factorVec, curr_Chains, MIN_FACTOR_LENGTH);
                StatPhase::log(" group count", groupVec.size());
                StatPhase::log(" factor count", factorVec.size());
                counter = 0;
                //start decrasing
            });

            curr_Chains.clear();
            curr_Chains.shrink_to_fit();
            phase2_buffer.clear();
            phase2_buffer.shrink_to_fit();
            len_t old_size = 0;

            //Phase 2
            std::vector <len_compact_t> active_index;
            StatPhase::wrap("Phase2", [&] {
                //StatPhase::log("Initial group count", groupVec.size());
                round = 0;
                while (!groupVec.empty()) {


                    StatPhase::wrap("Round: " + std::to_string(round), [&] {

                        StatPhase::wrap("findGroup", [&] { find_next_search_groups(groupVec, active_index); });

                        StatPhase::log("active_index size", active_index.size());

                        old_size = size;
                        StatPhase::wrap("fill hmap", [&] {
                            fill_group_hashmap(groupVec, hmap, factorVec, active_index, input_view, hash_provider,
                                               collision);
                        });
                        size = groupVec[active_index.front()].get_next_length() * 2;

                        StatPhase::log("hmap size", hmap.size());


                        StatPhase::wrap("search", [&] {
                            phase2_search(size, groupVec, hmap, factorVec, input_view, hash_provider, collision);
                        });
                        hmap = std::unordered_map<uint64_t, len_compact_t>();


                        StatPhase::wrap("check", [&] { check_groups(size, groupVec, active_index, factorVec); });

                        StatPhase::log("group count", groupVec.size());
                        active_index.clear();
                        groupVec.shrink_to_fit();
                        collision = false;

                    });
                    round++;

                }
            });
            std::sort(factorVec.begin(), factorVec.end(), std::greater<lz77Aprox::Factor>());


            lzss::FactorBufferRAM factors;
            // encode

            uint16_t threshold_ctz = __builtin_ctz(MIN_FACTOR_LENGTH);
            lz77Aprox::Factor f;
            len_t shrink_counter = 0;
            StatPhase::wrap("transfer3", [&] {
                while (!factorVec.empty()) {
                    if (shrink_counter == 4096 * 2) {
                        factorVec.shrink_to_fit();
                        shrink_counter = 0;
                    }
                    shrink_counter++;
                    f = factorVec.back();
                    factorVec.pop_back();

                    if (f.pos != f.src) {
                        factors.emplace_back(f.pos, f.src, f.len);
                        if (input_view.substr(f.pos, f.len) != input_view.substr(f.src, f.len)) {
                            std::cout << "fpos: " << f.pos << " fsrc: " << f.src << "f.len: " << f.len << std::endl;
                            std::cout << "fac: " << input_view.substr(f.pos, f.len) << std::endl
                                      << hash_provider->make_hash(f.pos, f.len, input_view) << std::endl;
                            std::cout << "src: " << input_view.substr(f.src, f.len) << std::endl
                                      << hash_provider->make_hash(f.src, f.len, input_view) << std::endl;
                            throw std::invalid_argument("forawrdref2:");
                        }

                        if (f.pos < f.src) {
                            std::cout << "fpos: " << f.pos << " fsrc: " << f.src << "f.len: " << f.len << std::endl;
                            std::cout << "fac: " << input_view.substr(f.pos, f.len) << std::endl;
                            std::cout << "src: " << input_view.substr(f.src, f.len) << std::endl;
                            throw std::invalid_argument("forawrdref:");
                        }
                    } else {
                        uint16_t hmap_index = hmap_storage.size() - 1 - (__builtin_ctz(f.len) - threshold_ctz);
                        uint64_t hash = hash_provider->make_hash(f.pos, f.len, input_view);
                        len_compact_t src = hmap_storage[hmap_index][hash];
                        len_t offset = 0;

                        //COLLISION
                        bool found = false;
                        while (!found) {
                            std::unordered_map<uint64_t, len_compact_t>::iterator iter = hmap_storage[hmap_index].find(
                                    hash + offset);
                            if (iter != hmap_storage[hmap_index].end()) {
                                src = iter->second;
                                if (input_view.substr(f.pos, f.len) == input_view.substr(src, f.len)) {
                                    //string matches
                                    factors.emplace_back(f.pos, src, f.len);
                                    found = true;
                                }
                            } else {
                                //this shouldnt happen
                                found = true;
                            }
                            offset++;
                        }


                    }


                }
            });

            factorVec.clear();
            factorVec.shrink_to_fit();
            //StatPhase::log("factors found", factors.size());
            len_t com_size = 0;
            len_t old_pos = 0;

            lzss::Factor old_fac;
            StatPhase::wrap("transfer4", [&] {
                for (lzss::Factor f : factors) {
                    if (old_pos > f.pos) {

                        throw std::invalid_argument("false ref2:");

                    }
                    old_fac = f;
                    old_pos = f.pos + f.len;
                    com_size = com_size + f.len;
                    if (input_view.substr(f.pos, f.len) != input_view.substr(f.src, f.len)) {
                        std::cout << "(" << f.pos << " , " << f.src << " , " << f.len << ")" << std::endl;
                        std::cout << "srcstring: " << input_view.substr(f.src, f.len) << std::endl;
                        std::cout << "orgstring: " << input_view.substr(f.pos, f.len) << std::endl;
                        throw std::invalid_argument("false ref2:");
                    }
                    if (f.pos == f.src) {
                        throw std::invalid_argument("false ref3:");
                    }
                }
            });
            StatPhase::log("cummutative factor length", com_size);
            delete hash_provider;
            StatPhase::wrap("Encode", [&] {
                auto coder = lzss_coder_t(config().sub_config("coder")).encoder(output,
                                                                                lzss::UnreplacedLiterals<decltype(input_view), decltype(factors)>(
                                                                                        input_view, factors));

                coder.encode_text(input_view, factors);
            });
        }

        inline std::unique_ptr <Decompressor> decompressor() const override {
            return Algorithm::instance < LZSSDecompressor < lzss_coder_t >> ();
        }
    };

} // namespace tdc
