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
#include <tudocomp/compressors/lz77Aprox/hash_interface_32.hpp>
//#include <tudocomp/compressors/lz77Aprox/stackoverflow_hash.hpp>
//#include <tudocomp/compressors/lz77Aprox/berenstein_hash.hpp>
//#include <tudocomp/compressors/lz77Aprox/dna_nth_hash.hpp>
#include <tudocomp/compressors/lz77Aprox/buz_hash.hpp>

namespace tdc {
    using namespace lz77Aprox;

    template<typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor {
        //this class seeks to implement an approximation of an LZ-parse
        //this result is achieved by implementing the first 2 phases of the algorithm described in
        //Approximating LZ77 via Small-Space Multiple-Pattern Matching
        //available at https://link.springer.com/chapter/10.1007/978-3-662-48350-3_45#Bib1

    private:
        //INPUT: a vector to be filled by chains, the size of the chains, the inputview
        //OUTPUT: the vector is filled with all initial chains
        //fills the given chain container with the right a mount of chains
        inline void
        populate_chainbuffer(std::vector <Chain> &curr_Chains, len_compact_t size, io::InputView &input_view) {
            //loop ofter the input until a pair of chains would reach beyond the input
            for (len_compact_t i = 0; i < input_view.size() - 1 - size * 2; i = i + size * 2) {

                curr_Chains.push_back(Chain(64, i));
                curr_Chains.push_back(Chain(64, i + size));

            }
        }

        //INPUT: a hashmap to be filled, a hashmap to store src_positions,
        //      a the vector of all non-cherry-chains, the current size
        //      the inputview, the hashprovider and a flag to set if collisions occur
        //OUTPUT: hashmap filled with th indices of all chains to still search,
        //          hashmap filled with already found src_positions
        //this function constructs the hashmap used in phase1_search
        inline void fill_chain_hashmap(std::unordered_map <uint32_t, len_compact_t> &hmap,
                                       std::unordered_map <uint32_t, len_compact_t> &hmap_storage,
                                       std::vector <Chain> &curr_Chains, len_compact_t size,
                                       io::InputView &input_view,
                                       hash_interface_32 *hash_provider, bool &collisions) {
            hmap.reserve(curr_Chains.size());


            uint32_t hash;
            std::unordered_map<uint32_t, len_compact_t>::iterator iter;

            //for all chains in curr_Chains do
            for (len_compact_t i = 0; i < curr_Chains.size(); i++) {

                hash = hash_provider->make_hash(curr_Chains[i].get_position(), size, input_view);

                iter = hmap.find(hash);

                //check if entry exists for hash
                if (iter == hmap.end()) {
                    //if not just construct entry
                    hmap[hash] = i;
                } else {
                    //NOT EMPTY
                    //entry exists
                    //Possible Collision
                    //check if strings match
                    if (input_view.substr(curr_Chains[iter->second].get_position(), size) ==
                        input_view.substr(curr_Chains[i].get_position(), size)) {
                        //no collision
                        if (curr_Chains[iter->second].get_position() < curr_Chains[i].get_position()) {
                            //entry is leftmost occurrence
                            curr_Chains[i].add(size);
                            insert_into_storage(hmap_storage, hash, curr_Chains[iter->second].get_position(), size,
                                                input_view);


                        } else {
                            //current/new chain is the leftmost occurrence
                            curr_Chains[iter->second].add(size);
                            hmap[hash] = i;
                            insert_into_storage(hmap_storage, hash, curr_Chains[i].get_position(), size,
                                                input_view);

                        }
                    } else {
                        //COLLISION
                        //strings dont match
                        //set the flag collisions to true
                        collisions = true;
                        bool inserted = false;
                        len_t offset = 1;

                        //loop until we find an empty bucket or a matching string
                        while (!inserted) {
                            iter = hmap.find(hash + offset);
                            if (iter == hmap.end()) {
                                //this bucket is empty
                                //insert
                                hmap[hash + offset] = i;
                                inserted = true;

                            } else {

                                // bucket is not empty
                                if (input_view.substr(curr_Chains[iter->second].get_position(), size) ==
                                    input_view.substr(curr_Chains[i].get_position(), size)) {
                                    //strings match
                                    //no collision
                                    if (curr_Chains[iter->second].get_position() < curr_Chains[i].get_position()) {
                                        //entry is leftmost occurrence
                                        curr_Chains[i].add(size);
                                        insert_into_storage(hmap_storage, hash,
                                                            curr_Chains[iter->second].get_position(), size,
                                                            input_view);

                                    } else {
                                        //new/current chain is leftmost occurrence
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


        inline void fill_chain_hashmap_no_store(std::unordered_map <uint32_t, len_compact_t> &hmap,
                                                std::vector <Chain> &curr_Chains, len_compact_t size,
                                                io::InputView &input_view,
                                                hash_interface_32 *hash_provider, bool &collisions) {
            hmap.reserve(curr_Chains.size());


            uint32_t hash;
            std::unordered_map<uint32_t, len_compact_t>::iterator iter;


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


                        } else {
                            //old lies behind
                            curr_Chains[iter->second].add(size);
                            hmap[hash] = i;


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


                                    } else {
                                        //old lies behind
                                        curr_Chains[iter->second].add(size);
                                        hmap[hash + offset] = i;


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

        //INPUT: hashmap to store src_positions, the hash,the index and the inputview
        //OUTPUT: hashmap contains the src_position
        //technically just a helper function to make the phase1_search method a little more readable
        inline void insert_into_storage(std::unordered_map <uint32_t, len_compact_t> &hmap, int64_t hash, len_t pos,
                                        len_compact_t size, io::InputView &input_view) {
            bool inserted = false;
            len_t offset = 0;
            std::unordered_map<uint32_t, len_compact_t>::iterator iter;
            //Loop until we find an empty bucket or a match
            //the match is not guaranteed to be an earlier src_position
            //because of the way we call this function in fill_chain_hash_map
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


        //INPUT: filled hashmap for the search, vector of all non-cherry-chains,
        //          the size of the chains, the inputview, the hash_provider
        //          a hashmap to store the src_positions,
        //          a flag if collisions occurred while constructing the hashmap
        //OUTPUT: an empty hashmap, all chains witch found src_positions marked with the size
        //          all src_positions inserted into second hashmap
        //this function performs a search for previous occurences
        // of the substrings corresponding to the chains
        inline void phase1_search(std::unordered_map <uint32_t, len_compact_t> &hmap,
                                  std::vector <Chain> &curr_Chains,
                                  len_compact_t size, io::InputView &input_view, hash_interface_32 *hash_provider,
                                  std::unordered_map <uint32_t, len_compact_t> &hmap_storage, bool &coll) {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_compact_t index;

            std::unordered_map<uint32_t, len_compact_t>::iterator iter;
            int last = 0;
            len_t collisions = 0;
            //if collisions occured while constructing the hashmap
            if (coll) {
                //search over the whole input
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    last++;
                    //check if an entry exists for the current hashvalue
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        //check for collisions
                        if (input_view.substr(curr_Chains[index].get_position(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //NO COLLISION
                            //check if we have "rolled" beyond the chains
                            if (rhash.position < curr_Chains[index].get_position()) {
                                curr_Chains[index].add(size);
                                insert_into_storage(hmap_storage, rhash.hashvalue, rhash.position, size,
                                                    input_view);
                            }
                            //check if the following bucket contains a colliding string
                            if (hmap.find(rhash.hashvalue + 1) != hmap.end()) {
                                len_t offset = 1;
                                len_t dex = hmap[rhash.hashvalue + offset];
                                len_t ss = curr_Chains[dex].positon;
                                //if the next bucket contains a colliding string also check all further
                                //buckets until an empty one is found
                                if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                    bool empty_found = false;
                                    while (!empty_found) {
                                        if (hmap.find(rhash.hashvalue + offset) != hmap.end()) {
                                            //check if hash match
                                            dex = hmap[rhash.hashvalue + offset];
                                            ss = curr_Chains[dex].positon;
                                            if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                                //"move" entry
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
                            //first entry collides
                            collisions++;
                            len_t offset = 1;
                            bool mt_or_right_found = false;
                            //loop over all following buckets until we find an empty one
                            while (!mt_or_right_found) {
                                iter = hmap.find(rhash.hashvalue + offset);
                                if (iter != hmap.end()) {
                                    if (input_view.substr(curr_Chains[index].get_position(), size) ==
                                        input_view.substr(rhash.position, size)) {
                                        //STRING MATCHES
                                        if (rhash.position < curr_Chains[index].get_position()) {
                                            curr_Chains[index].add(size);

                                        }

                                        //store src_position
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
            }
                //if no collisions occured while constructing the hashmap
            else {
                //search over the whole input
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    //check if entry exits for current hashvalue
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        //check for collisions
                        if (input_view.substr(curr_Chains[index].get_position(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //TRUE MATCH no collision
                            //check if we are still to the left of the chain
                            if (rhash.position < curr_Chains[index].get_position()) {
                                curr_Chains[index].add(size);
                                insert_into_storage(hmap_storage, rhash.hashvalue, rhash.position, size,
                                                    input_view);
                            }
                            //either way we dont need to consider this entry any longer
                            hmap.erase(rhash.hashvalue);
                        }
                    }

                    //advance the rolling hash
                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            }


        }


        inline void phase1_search_no_store(std::unordered_map <uint32_t, len_compact_t> &hmap,
                                           std::vector <Chain> &curr_Chains,
                                           len_compact_t size, io::InputView &input_view,
                                           hash_interface_32 *hash_provider,
                                           bool &coll) {

            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);
            len_compact_t index;

            std::unordered_map<uint32_t, len_compact_t>::iterator iter;
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
                            }
                            hmap.erase(rhash.hashvalue);
                        }
                    }


                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            }


        }

        //INPUT: vector of all non-cherry-chains, size last searched, vector to store cherry-chains
        //OUTPUT: vector of chains no longer contains cherry-chains, chains have been split or adjusted
        //function prepares chain vector for the next round
        inline void
        new_chains(std::vector <Chain> &Chains, len_compact_t size, std::vector <Chain> &phase2_buffer) {

            Chain left;
            bool curr_left;
            Chain right;
            bool curr_right;
            len_compact_t curr_size;

            len_compact_t oldsize = Chains.size();

            //check all chains in pair of 2
            for (len_compact_t i = 0; i < oldsize && i < Chains.size(); i = i + 2) {

                left = Chains[i];
                right = Chains[i + 1];
                //check if a factor was found in this round
                curr_left = left.get_chain() & size;
                curr_right = right.get_chain() & size;

                //only insert new chains if neither are found
                //add 2 new chains at the end for the right child
                //and reuse the old chains for the left child
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
                //CHERRY!
                //we can move both chains to the phase2_buffer
                //replace them with other chains to avoid vector element reallocation
                if (curr_left && curr_right) {


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
                //only left found
                //just adjust the chains
                if (curr_left) {

                    Chains[i].set_position(right.get_position());

                    Chains[i + 1].set_position(right.get_position() + size / 2);

                } else {
                    //only rightfound
                    //just adjust the chains

                    Chains[i + 1].set_position(left.get_position() + size / 2);

                }
            }
            Chains.shrink_to_fit();
        }

        //INPUT: vector of all Groups , vector to be filled with indices
        //OUTPUT: vector is filled with indices of all Groups with
        //          shortest searchstring
        inline void
        find_next_search_groups(std::vector <Group> &groupVec, std::vector <len_compact_t> &active_index) {
            len_t size = groupVec.front().get_next_length() * 2;

            //simple loop
            //keep shortest searchstrings
            //if smaller is found clear vector and continue
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

        //INPUT: vector of all Groups, hashmap to be filled,
        //            vector of Factors to be filled, vector of indices of groups to be inserted,
        //              the inputview, the hashprovider, a flag to set if collisions occur
        //OUTPUT:  hashmap is filled with hashes and indices,
        //          Groups which searchstrings  reach beyond the file(at most one) have their active group/factor added to the factorVec
        //          Groups which are already found have absorped their next factor
        //function fill the hashmap and reports if collisions occured
        inline void fill_group_hashmap(std::vector <Group> &groupVec,
                                       std::unordered_map <uint32_t, len_compact_t> &hmap,
                                       std::vector <lz77Aprox::Factor> &factorVec,
                                       std::vector <len_compact_t> &active_index, io::InputView &input_view,
                                       hash_interface_32 *hash_provider, bool &collisions) {

            len_t size = groupVec[active_index.front()].get_next_length() * 2;
            uint32_t hash;
            std::unordered_map<uint32_t, len_compact_t>::iterator iter;


            for (len_compact_t i : active_index) {

                len_t k = groupVec[i].get_start_of_search();
                //if the groups searchstring reaches beyond the file
                //(which can happen because we double the length of next factor)
                // dont insert it
                if (k + size < input_view.size()) {
                    //make hash for group searchstring
                    hash = hash_provider->make_hash(k, size, input_view);

                    iter = hmap.find(hash);
                    //check if entry exists
                    if (iter == hmap.end()) {
                        hmap[hash] = i;

                    } else {
                        //POSSIBLE COLLISION
                        //entyr exists

                        if (input_view.substr(groupVec[i].get_start_of_search(), size) ==
                            input_view.substr(groupVec[iter->second].get_start_of_search(), size)) {
                            //NO COLLISION strings match
                            //check which one is the leftmost
                            len_t old = groupVec[iter->second].get_start_of_search();

                            if (k < old) {

                                groupVec[iter->second].absorp_next(k);
                                hmap[hash] = i;

                            } else {
                                groupVec[i].absorp_next(old);

                            }
                        } else {
                            //COLLISION
                            //strings dont match
                            //iterate over all the following buckets
                            //until we find a match or insert the string
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
                                        //check which one is the left most one
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

        //INPUT: size of searchstrings, vector of all Groups, hashmap for searching
        //         vector to be filled with Factors, the input_view, the hash_provider
        //         flag if we have collisions inside the hashmap
        //OUTPUT: empty hashmap, all Groups with found searchsting have absorbed their next factor,
        //          all Groups which haven't had their searchstring found had  thier active group/factor
        //          added to factorVec
        inline void
        phase2_search(len_compact_t size, std::vector <Group> &groupVec,
                      std::unordered_map <uint32_t, len_compact_t> &hmap,

                      std::vector <lz77Aprox::Factor> &factorVec, io::InputView &input_view,
                      hash_interface_32 *hash_provider, bool &coll) {
            rolling_hash rhash = hash_provider->make_rolling_hash(0, size, input_view);

            std::unordered_map<uint32_t, len_compact_t>::iterator iter;
            len_compact_t index;
            //if the hashmap had collisions
            if (coll) {
                //search over the whole input
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);

                    //check if an entry exists for the current hashvalue
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        //check if the found string produces a collision
                        if (input_view.substr(groupVec[index].get_start_of_search(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //NO COLLISSION -> check if the position is to the left of the searchstring
                            if (rhash.position < groupVec[index].get_start_of_search()) {
                                //if yes then we have found a src_position
                                //the avtive group/factor can merge with the next factor
                                groupVec[index].absorp_next(rhash.position);

                            } else {
                                //if not we have "rolled" to the right of the original searchstring
                                // so we can add its active group/factor to the factorVec
                                factorVec.push_back(groupVec[index].advance_Group());
                            }
                            //check if there are colliding searchstrings in the following bucket(SINGULAR!)
                            if (hmap.find(rhash.hashvalue + 1) != hmap.end()) {
                                len_t offset = 1;
                                len_t dex = hmap[rhash.hashvalue + offset];
                                len_t ss = groupVec[dex].get_start_of_search();
                                if (rhash.hashvalue == hash_provider->make_hash(ss, size, input_view)) {
                                    //if the next bucket is filled if a colliding string there may be more
                                    //so we need to loop until we find an empty bucket
                                    bool empty_found = false;
                                    while (!empty_found) {
                                        if (hmap.find(rhash.hashvalue + offset) != hmap.end()) {
                                            //check if hashes match
                                            dex = hmap[rhash.hashvalue + offset];
                                            ss = groupVec[dex].get_start_of_search();
                                            //if we find a colliding searchstring "move it 1 forward"
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
                                    //if there a no colliding strings delete the entry
                                    hmap.erase(rhash.hashvalue);
                                }

                            } else {
                                //if there a no colliding strings delete the entry
                                hmap.erase(rhash.hashvalue);
                            }


                        } else {
                            //the original entry is a collisions
                            //check all following buckets for matching strings
                            len_t offset = 1;
                            bool mt_or_right_found = false;
                            while (!mt_or_right_found) {
                                iter = hmap.find(rhash.hashvalue + offset);
                                if (iter != hmap.end()) {
                                    index = iter->second;
                                    if (input_view.substr(groupVec[index].get_position(), size) ==
                                        input_view.substr(rhash.position, size)) {
                                        //STRING MATCHES for the bucket
                                        if (rhash.position < groupVec[index].get_start_of_search()) {
                                            //if yes then we have found a src_position
                                            //the active group/factor can merge with the next factor
                                            groupVec[index].absorp_next(rhash.position);
                                        } else {
                                            //if not we have "rolled" to the right of the original searchstring
                                            // so we can add its active group/factor to the factorVec one
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
                    //advance the rollinghash
                    hash_provider->advance_rolling_hash(rhash, input_view);
                }
            }
                //if the hashmap does not have any collisions
            else {
                //search over the whole input
                for (len_compact_t i = size; i < input_view.size(); i++) {

                    iter = hmap.find(rhash.hashvalue);
                    //check if the hashmap has an entry for the current rhash value
                    if (iter != hmap.end()) {
                        index = hmap[rhash.hashvalue];
                        //check if the found string is a collision
                        if (input_view.substr(groupVec[index].get_start_of_search(), size) ==
                            input_view.substr(rhash.position, size)) {
                            //if it is not a collision
                            //check if the potential src_position lies to the left of the string
                            if (rhash.position < groupVec[index].get_start_of_search()) {
                                //if yes then we ahve found a src_position
                                //the avtive group/factor can merge with the next factor
                                groupVec[index].absorp_next(rhash.position);
                            } else {
                                //if not we have "rolled" to the right of the original searchstring
                                // so we can add its active group/factor to the factorVec

                                factorVec.push_back(groupVec[index].advance_Group());
                            }
                            //Either way we dont need to check for it anymore
                            //this works for all cause the rolling hash runs over each one
                            hmap.erase(rhash.hashvalue);
                        }
                    }
                    //advance the rollinghash
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


        //INPUT:: length of serchstring, vector of all Groups, vector of indices of Groups to check
        //          vector to be filled with Factors
        //OUTPUT:: all Groups with no longer encode more then 1 Factor have ben removed from groupVec
        //          factorVec has been filled with all of the active groups/factors of the deleted Groups
        // function checks if the Groups still need processing or if we are done with them
        inline void
        check_groups(len_t size, std::vector <Group> &groupVec, std::vector <len_compact_t> &active_index,
                     std::vector <lz77Aprox::Factor> &factorVec) {


            std::vector <len_compact_t> mark_delete;

            // for all Groups which took part in the search
            // check if the encode more then 1 Factor
            //if NOT then add the active group/factor of the Group to factorVec
            //and "mark" it for deletion
            for (len_compact_t index : active_index) {

                Group g = groupVec[index];
                if (!g.has_next()) {

                    mark_delete.push_back(index);
                    factorVec.push_back(g.advance_Group());
                }
            }
            //delete all marked

            //this loop deletes the "marked" Groups
            len_compact_t index;
            while (!mark_delete.empty()) {
                index = mark_delete.back();

                //instead of calling delete() simple replace the Groups
                //with the last Group in the vector
                //this prevents the reallocation and shifting around of all
                //vector elements behind this one
                //if this one is the last just delete it

                if (groupVec.size() - 1 == index) {
                    groupVec.pop_back();
                } else {
                    groupVec[index] = groupVec.back();
                    groupVec.pop_back();
                }
                mark_delete.pop_back();
            }

        }

        //INPUT: vector to be filled with Groups, vector to befilled with Factors
        //       vector of cherry-chains to be consumed, the last size searched for
        //OUTPUT: vectors are filled with groups/factors, phase2_buffer is empty
        //
        //function to transform cherry-chains to groups,factors or to delete them
        inline void cherrys_to_groups(std::vector <Group> &groupVec, std::vector <lz77Aprox::Factor> &factorVec,
                                      std::vector <Chain> &phase2_buffer, len_t threshold) {

            Chain c;

            //phase2_buffer has structure decreasing,increasing,decreasing...
            // just toggle this bool for every chain processed
            // start with decreasing
            bool inc = true;

            //this should limit the memory needs
            //adjust for time/size
            len_t shrink_counter = 0;
            len_t shrinkmax = phase2_buffer.size() / 200;
            while (!phase2_buffer.empty()) {

                //adjust memory needs
                if (shrink_counter == shrinkmax) {
                    phase2_buffer.shrink_to_fit();
                    shrink_counter = 0;
                }
                shrink_counter++;
                c = phase2_buffer.back();

                //chain encodes only one factor -> skip phase 2
                //add factor directly to the factorVec

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
                    //get the size of the "cherry strings"
                    len_t lc = 1 << __builtin_ctz(c.get_chain());
                    //generate Group
                    groupVec.push_back(Group(c, inc, lc));

                    inc = !inc;
                    phase2_buffer.pop_back();

                    continue;
                }
            }
            phase2_buffer.shrink_to_fit();
        }

        //INPUT: vector to be filled with Groups, vector to befilled with Factors
        //       vector of chains to be consumed, the last size searched for
        //OUTPUT: vectors are filled with groups/factors, curr_Chains is empty
        //
        //function to transform normal chains to groups,factors or to delete them
        inline void chains_to_groups(std::vector <Group> &groupVec, std::vector <lz77Aprox::Factor> &factorVec,
                                     std::vector <Chain> &curr_Chains, len_t threshold) {

            //curr_chains has structure decreasing,increasing,decreasing...
            // just toggle this bool for every chain processed
            bool inc = true;
            Chain c;
            //this should limit the memory needs
            //adjust for time/size
            len_t shrink_counter = 0;
            len_t shrinkmax = curr_Chains.size() / 200;
            while (!curr_Chains.empty()) {

                //adjust memory needs
                if (shrink_counter == shrinkmax) {
                    curr_Chains.shrink_to_fit();
                    shrink_counter = 0;
                }
                c = curr_Chains.back();
                shrink_counter++;
                if (1 == __builtin_popcount(c.get_chain())) {
                    //chain encodes only one factor -> skip phase 2
                    //add factor directly to the factorVec

                    //check if position is correct or needs adjusting
                    //if last_set is true then the string was found in the last round and position is still correct
                    //if last_set is false then the string was found in any  previous round and we need to adjust the position
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
                }
                if (c.get_chain()) {
                    //chain is not null and doesnt have a popcount of 1
                    // this means that the chain encodes 2 or more factors
                    //"enqueue" the chain as a group for phase 2
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

        //INPUT: the file as a view, the output stream
        //OUTPUT: 5-optimal LZ-parse approximation encoded with given encoder
        //"main" function
        inline virtual void compress(Input &input, Output &output) override {
            //set input and output
            io::InputView input_view = input.as_view();
            auto os = output.as_stream();

            //get parameters
            len_compact_t WINDOW_SIZE = config().param("window").as_int() * 2;
            len_compact_t MIN_FACTOR_LENGTH = config().param("threshold").as_int();
            if(__builtin_popcount(WINDOW_SIZE)!=1 || __builtin_popcount(MIN_FACTOR_LENGTH)!=1){
                throw std::invalid_argument("parameters are not a power of 2");
            }


            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE * 2) {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            //choose HASH
            hash_interface_32 *hash_provider;

            hash_provider = new berenstein_hash();
            //hash_provider = new dna_nth_hash();
            //hash_provider = new stackoverflow_hash();
            //hash_provider = new buz_hash();


            //make reusable containers
            //INIT: Phase 1
            //contains all chains which are not cherrys
            std::vector <Chain> curr_Chains;
            //contains the cherry-chains
            std::vector <Chain> phase2_buffer;
            //hashmap for searching
            std::unordered_map <uint32_t, len_compact_t> hmap;
            //vector of hashmaps
            //hashmaps contain found src_positions
            //sorted from big to small
            std::vector <std::unordered_map<uint32_t, len_compact_t>> hmap_storage;


            std::unordered_map <uint32_t, len_compact_t> temp_hmap_storage;


            len_compact_t size = WINDOW_SIZE / 2;
            len_compact_t round = 0;
            bool collision = false;

            //generate initial chains
            populate_chainbuffer(curr_Chains, size, input_view);

            StatPhase::wrap("Phase1", [&] {
                StatPhase::log("Initial chain count", curr_Chains.size());
                while (size >= MIN_FACTOR_LENGTH) {
                    StatPhase::wrap("Round: " + std::to_string(round), [&] {

                        StatPhase::wrap("fill hashmap", [&] {
                            fill_chain_hashmap(hmap, temp_hmap_storage, curr_Chains, size, input_view,
                                               hash_provider,
                                               collision);

                        });

                        //log stats
                        StatPhase::log("collisions", collision);
                        StatPhase::log("hmap.size", hmap.size());

                        StatPhase::wrap("Search", [&] {
                            phase1_search(hmap, curr_Chains, size, input_view, hash_provider, temp_hmap_storage,
                                          collision);

                        });

                        //save all found src_positions
                        hmap_storage.push_back(std::move(temp_hmap_storage));

                        //make new hashmaps
                        // just clear() doesnt free the memory
                        temp_hmap_storage = std::unordered_map<uint32_t, len_compact_t>();
                        hmap = std::unordered_map<uint32_t, len_compact_t>();



                        // on the last level this doesnt need to be done
                        // since we dont search for strings less than MIN_FACTOR_LENGTH
                        if (size != MIN_FACTOR_LENGTH) {
                            StatPhase::wrap("make new chains",
                                            [&] { new_chains(curr_Chains, size, phase2_buffer); });
                        }
                        //log number of chains after round
                        StatPhase::log("chain count", curr_Chains.size());

                        size = size / 2;
                        round++;
                        collision = false;
                    });

                }
            });

            //make containers for phase 2
            std::vector <Group> groupVec;
            std::vector <lz77Aprox::Factor> factorVec;

            // make groups out of the cherry-chains
            len_t counter = 0;
            StatPhase::wrap("transfer1", [&] {
                StatPhase::log("cherry count", phase2_buffer.size());
                //add all cherry in phase 2 buffer to groupvec
                cherrys_to_groups(groupVec, factorVec, phase2_buffer, MIN_FACTOR_LENGTH);
            });

            //log the number of groups we generatet from the cherry-chains
            StatPhase::log("chain group count", groupVec.size());
            //log the number of factors we got from the cherry-chains
            StatPhase::log("chain factor count", factorVec.size());

            //make groups from normal chains
            StatPhase::wrap("transfer2", [&] {
                chains_to_groups(groupVec, factorVec, curr_Chains, MIN_FACTOR_LENGTH);
                StatPhase::log(" group count", groupVec.size());
                StatPhase::log(" factor count", factorVec.size());
                counter = 0;
                //start decrasing


                curr_Chains.clear();
                curr_Chains.shrink_to_fit();
                phase2_buffer.clear();
                phase2_buffer.shrink_to_fit();
            });


            //Phase 2
            //make container for groups with shortest searchstring
            std::vector <len_compact_t> active_index;
            StatPhase::wrap("Phase2", [&] {
                StatPhase::log("Initial group count", groupVec.size());
                round = 0;

                //run until all groups are empty
                while (!groupVec.empty()) {


                    StatPhase::wrap("Round: " + std::to_string(round), [&] {

                        StatPhase::wrap("findGroup", [&] { find_next_search_groups(groupVec, active_index); });

                        StatPhase::log("active_index size", active_index.size());


                        StatPhase::wrap("fill hmap", [&] {
                            fill_group_hashmap(groupVec, hmap, factorVec, active_index, input_view, hash_provider,
                                               collision);
                        });
                        //size of searchstrings
                        size = groupVec[active_index.front()].get_next_length() * 2;

                        StatPhase::log("hmap size", hmap.size());
                        StatPhase::log("collisions", collision);


                        StatPhase::wrap("search", [&] {
                            phase2_search(size, groupVec, hmap, factorVec, input_view, hash_provider, collision);
                        });
                        hmap = std::unordered_map<uint32_t, len_compact_t>();


                        StatPhase::wrap("check", [&] { check_groups(size, groupVec, active_index, factorVec); });

                        //log number of groups remaining
                        StatPhase::log("group count", groupVec.size());

                        active_index.clear();
                        groupVec.shrink_to_fit();
                        collision = false;

                    });
                    round++;

                }
            });
            std::sort(factorVec.begin(), factorVec.end(), std::greater<lz77Aprox::Factor>());

            //make encode container
            lzss::FactorBufferRAM factors;
            // encode


            StatPhase::wrap("transfer3", [&] {

                len_t threshold_ctz = __builtin_ctz(MIN_FACTOR_LENGTH);
                lz77Aprox::Factor f;
                len_t shrink_counter = 0;
                len_t shrinkmax=factorVec.size()/25;
                while (!factorVec.empty()) {
                    //adjust memory needs
                    if (shrink_counter == shrinkmax) {
                        factorVec.shrink_to_fit();
                        shrink_counter = 0;
                    }

                    shrink_counter++;
                    f = factorVec.back();
                    factorVec.pop_back();

                    if (f.pos != f.src) {
                        factors.emplace_back(f.pos, f.src, f.len);
                    } else {
                        len_t hmap_index = hmap_storage.size() - 1 - (__builtin_ctz(f.len) - threshold_ctz);
                        uint32_t hash = hash_provider->make_hash(f.pos, f.len, input_view);
                        len_compact_t src = hmap_storage[hmap_index][hash];
                        len_t offset = 0;

                        //COLLISION
                        bool found = false;
                        while (!found) {
                            std::unordered_map<uint32_t, len_compact_t>::iterator iter = hmap_storage[hmap_index].find(
                                    hash + offset);
                            if (iter != hmap_storage[hmap_index].end()) {
                                src = iter->second;
                                if (input_view.substr(f.pos, f.len) == input_view.substr(src, f.len)) {
                                    //string matches
                                    factors.emplace_back(f.pos, src, f.len);
                                    found = true;
                                }
                            } else {
                                //this shouldn't happen
                                found = true;
                            }
                            offset++;
                        }
                    }
                }

            });


            //delete and clear what containers and objects remain

            factorVec.clear();
            factorVec.shrink_to_fit();



            // encode



            len_t com_size = 0;
            len_t old_pos = 0;

            lzss::Factor old_fac;
            // check if the generated lz-parse is correct
            //crash if it is not
            for (lzss::Factor f : factors) {
                if (old_pos > f.pos) {

                    throw std::invalid_argument("false ref: factors not unique");

                }
                old_fac = f;
                old_pos = f.pos + f.len;
                com_size = com_size + f.len;
                if (input_view.substr(f.pos, f.len) != input_view.substr(f.src, f.len)) {
                    std::cout << "(" << f.pos << " , " << f.src << " , " << f.len << ")" << std::endl;
                    std::cout << "srcstring: " << input_view.substr(f.src, f.len) << std::endl;
                    std::cout << "srchash: " << hash_provider->make_hash(f.src,f.len,input_view) << std::endl;
                    std::cout << "orgstring: " << input_view.substr(f.pos, f.len) << std::endl;
                    std::cout << "orghash: " << hash_provider->make_hash(f.pos,f.len,input_view) << std::endl;
                    throw std::invalid_argument("false ref: pos.str != src.str");
                }
                if (f.pos == f.src) {
                    throw std::invalid_argument("false ref: pos == src");
                }
            }

            StatPhase::log("cumulative factor length", com_size);

            StatPhase::wrap("Encode", [&] {
                auto coder = lzss_coder_t(config().sub_config("coder")).encoder(output,
                                                                                lzss::UnreplacedLiterals<decltype(input_view), decltype(factors)>(
                                                                                        input_view, factors));

                coder.encode_text(input_view, factors);
            });
            delete hash_provider;
        }

        //lzss decompressor
        inline std::unique_ptr <Decompressor> decompressor() const override {
            return Algorithm::instance < LZSSDecompressor < lzss_coder_t >> ();
        }
    };

} // namespace tdc
