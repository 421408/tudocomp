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
#include <string>
#include <tuple>
#include <iostream>

#include <tudocomp/def.hpp>

#include <tudocomp/compressors/lzss/FactorBuffer.hpp>
#include <tudocomp/compressors/lzss/UnreplacedLiterals.hpp>

#include <tudocomp/compressors/lz77Aprox/AproxFactor.hpp>
#include <tudocomp/compressors/lz77Aprox/Cherry.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
#include <tudocomp/compressors/lz77Aprox/stackoverflow_hash.hpp>

namespace tdc
{
    template <typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor
    {

    private:
        struct cherry_child_ref
        {
            bool right;
            std::set<Cherry>::iterator index;
        };

        struct hmap_value
        {
            cherry_child_ref first_factor;
           
        };

        //this is ugly
        len_t WINDOW_SIZE = 2048  ;
        const len_t MIN_FACTOR_LENGTH = 4;

        std::set<Cherry> FactorBuffer;
        hash_interface *hash_provider;

        rolling_hash rhash;

        inline long long get_factor_hash(AproxFactor &fac, io::InputView &input_view) { return hash_provider->make_hash(fac.position, fac.length, input_view); }

        inline long long get_factor_hash_left(Cherry &fac, io::InputView &input_view) { return hash_provider->make_hash(fac.position, (fac.length / 2), input_view); }

        inline long long get_factor_hash_right(Cherry &fac, io::InputView &input_view) { return hash_provider->make_hash(fac.position + fac.length / 2, fac.length / 2, input_view); }

        //this function fills the FactorBuffer with the "first row" of the "tree"
        //TODO typedef FactorFlags cause they are ugly now (and unreadable)
        void populate_buffer(len_t window_size, io::InputView &input_view)
        {
            len_t position(0);
            //run until the next factor would reach beyond the file

            for (len_t i = 0; i <= input_view.size() - 1 - window_size; i = i + window_size)
            {

                FactorBuffer.emplace(Cherry(position, window_size, 0));
                ;

                //cant use i after scope but must know pos for eof handle
                //if you use position in loop last step will be missing and you dont know if it its exactly
                position = position + window_size;
            }

            //marks the last factor as eof-factor if the Factor border lies beyond the end of the file
            //note that only the very last Factor in the Buffer can have flag 7 so only ever check the last factor for this case
            if (position < input_view.size())
            {
                FactorBuffer.emplace(Cherry(position, window_size, 7));
            }
        }

        //scans the FactorBuffer for all cherrys to be tested
        void make_active_cherry_list(std::vector<std::set<Cherry>::iterator> &vec)
        {
            for (auto iter = FactorBuffer.begin(); iter != FactorBuffer.end(); iter++)
            {
                if ((*iter).status == 0 || (*iter).status == 7)
                {
                    vec.push_back(iter);
                }
            }
        }

        //searches with the rolling hash over the view and marks all children of the cherrys if found
        void mark_cherrys(std::unordered_map<long long, hmap_value> &hmap, std::unordered_map<long long, len_t> &storage, io::InputView &input_view)
        {
            //dont let the hash rollbeyond the view
            //main loop over the entire view
            for (len_t i = 0; i < input_view.size() - 1 - rhash.length; i++)
            {

                //check if key exists
                if (hmap.end() != hmap.find(rhash.hashvalue))
                {
                    if (hmap[rhash.hashvalue].first_factor.right)
                    {
                        if (rhash.position < (*hmap[rhash.hashvalue].first_factor.index).position +
                                                 (*hmap[rhash.hashvalue].first_factor.index).length / 2)
                        {
                            //rolling hash matches and lies before the first factor
                            
                            (*hmap[rhash.hashvalue].first_factor.index).rightfound = true;
                        }
                    }
                    else
                    {
                        //left factor
                        if (rhash.position < (*hmap[rhash.hashvalue].first_factor.index).position)
                        {
                            //rolling hash matches and lies before the first factor
                            
                            (*hmap[rhash.hashvalue].first_factor.index).leftfound = true;
                        }
                    }

                    //storage
                    storage[rhash.hashvalue] = rhash.position;

                    //rolling hash reached the first occurence
                    // we cant find it beyond here
                    //so remove it from the hmap
                    hmap.erase(rhash.hashvalue);
                }
                hash_provider->advance_rolling_hash(rhash,input_view);
            }
            hmap.clear();
        }

        //inserts the childern of a factor correctly into an unordered map
        //to correnctly set the first occurence insert factors from left to right
        void insert_factor(std::unordered_map<long long, hmap_value> &hmap, std::set<Cherry>::iterator &index, io::InputView &input_view)
        {

            //doesnt work otherwise dunno why
            Cherry k = *index;
            long long hash = get_factor_hash_left(k, input_view);

            //std::cout << "1" << std::endl;
            //left key exists
            if (hmap.end() != hmap.find(hash))
            {
                //if key exist before this one we only need to search thefirst of those
                (*index).leftfound = true;
            }
            else
            {
                //left key doesnt exist before set first_occ to position of factor
                hmap[hash] = hmap_value{cherry_child_ref{false, index}};
            }
            hash = get_factor_hash_right(k, input_view);
            //std::cout << "2" << std::endl;
            //right key exists
            if (hmap.end() != hmap.find(hash))
            {
                //if key exist before this one we only need to search thefirst of those
                (*index).rightfound = true;
            }
            else
            {
                //right key doesnt exist before set first_occ to position of factor
                hmap[hash] = hmap_value{cherry_child_ref{true, index}};
            }
        }

        void populate_multimap(std::unordered_map<long long, hmap_value> &hmap, std::vector<std::set<Cherry>::iterator> &cherrylist, io::InputView &input_view)
        {
            for (len_t iter = 0; iter < cherrylist.size() - 1; iter++)
            {
                insert_factor(hmap, cherrylist[iter], input_view);
            }
            Cherry c = (*cherrylist.back());
            //handle eof
            if ((*cherrylist.back()).status == 7)
            {
                len_t bull = c.position;
                len_t shit = c.length / 2;
                len_t bullshit = bull + shit - 1; //-1 to account for the fact that we dont start at 0

                if (bullshit >= input_view.size() - 1)
                {
                    //leftchild also reaches Beyond
                    c.split();
                    c.status = 7;
                    //erase old Cherry
                    FactorBuffer.erase(cherrylist.back());
                    //add new Cherry
                    FactorBuffer.insert(c);
                    //new Cherry ist active and iter is invalid
                    cherrylist.pop_back();
                }
                else
                {
                    //only the right child reaches into the beyond
                    long long hash = get_factor_hash_left(c,input_view);
                    //left key exists
                    if (hmap.end() != hmap.find(hash))
                    {
                        cherrylist.back()->leftfound = true;
                    }
                    else
                    {
                        //left key doesnt exist before set first_occ to position of factor
                        hmap[hash] = hmap_value{cherry_child_ref{false, cherrylist.back()}};
                    }
                }
            }
            else
            {
                // last one isnt eof factor
                insert_factor(hmap, cherrylist.back(), input_view);
            }
        }

        void apply_findings(std::vector<std::set<Cherry>::iterator> &cherrylist)
        {

            Cherry c;

            for (std::set<Cherry>::iterator iter : cherrylist)
            {

                switch ((*iter).rightfound * 1 + (*iter).leftfound * 2)
                {
                case 0:
                    //neither left nor right found -> new cherry
                    FactorBuffer.emplace((*iter).split());

                    break;
                case 1:
                    //only right found
                    (*iter).only_rightfound();

                    break;
                case 2:
                    //only leftfound
                    c = (*iter);
                    FactorBuffer.erase(iter);
                    c.only_leftfound();
                    FactorBuffer.insert(c);

                    break;
                case 3: //cherrydone
                    (*iter).status = 1;

                    break;
                }
            }
        }


        void inflate_chains(std::vector<AproxFactor> &facbuf, io::InputView &input_view)
        {

            unsigned long long k = 0;

            len_t length = 0;
            len_t position = 0;
            Cherry c;
            Cherry last = *(std::prev(FactorBuffer.end()));
            FactorBuffer.erase(std::prev(FactorBuffer.end()));
            //std::cout << "tt test" << std::endl;
           while(!FactorBuffer.empty())
            {
                c=(*FactorBuffer.begin());
                FactorBuffer.erase(FactorBuffer.begin());
                while (c.l > 0)
                {
                    k = (63 - __builtin_clzll(c.l));
                    length = 1 << k;
                    facbuf.push_back(AproxFactor(position, length, 2));
                    c.l = c.l - length;
                    position = position + length;
                }

                if(position!=c.position){
                    std::cout <<"pos: "<<position<<"c.pos: "<<c.position<<std::endl;
                    throw std::invalid_argument( "not ordered" );
                }

                facbuf.push_back(AproxFactor(c.position, c.length / 2, 2));
                facbuf.push_back(AproxFactor(c.position + c.length / 2, c.length / 2, 2));

                position = c.position + c.length;
                while (c.r > 0)
                {
                    k = (__builtin_ctzll(c.r));
                    length = 1 << k;
                    facbuf.push_back(AproxFactor(position, length, 2));
                    c.r = c.r - length;
                    position = position + length;
                }
                
            }
            //eof
            while (last.l > 0)
            {
                k = (63 - __builtin_clzll(last.l));
                length = 1 << k;
                facbuf.push_back(AproxFactor(position, length, 2));
                last.l = last.l - length;
                position = position + length;
            }
            facbuf.push_back(AproxFactor(last.position, input_view.size() - last.position, 2));
        }

        len_t tree_level_by_size(len_t size)
        {
            len_t i = 0;
            len_t ws = WINDOW_SIZE / 2; //half because we test the children not the thing itself
            while (ws != size)
            {
                ws = ws / 2;
                i++;
            }
            return i;
        }

        void insert_first_occ(std::vector<AproxFactor> &facbuf, std::vector<std::unordered_map<long long,len_t>> &map_storage, io::InputView &input_view)
        {
            len_t x= __builtin_clz(WINDOW_SIZE/2);
            for (len_t i = 0; i < facbuf.size(); i++)
            {

                AproxFactor c = facbuf[i];
                //get the right bucket
                len_t k = __builtin_clz(c.length)-x;;
                //skip literals
                if (k > map_storage.size() - 1)
                {
                    continue;
                }
                long long h = get_factor_hash(c, input_view);

                if (map_storage[k].end() != map_storage[k].find(h))
                {
                    if((map_storage[k])[h]>facbuf[i].position){
                        std::cout<<"src: "<<(map_storage[k])[h]<<" pos: "<<facbuf[i].position<<std::endl;
                        std::cout<<"src: "<<input_view.substr( (map_storage[k])[h], facbuf[i].length)<<" pos: "<< input_view.substr(facbuf[i].position,facbuf[i].length)<<std::endl;
                        throw std::invalid_argument( "received negative value" );
                    }
                    facbuf[i].firstoccurence = (map_storage[k])[h];
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

            std::cout << "hi" << std::endl;
            std::cout << input_view.size() << std::endl;
            std::cout << int(input_view[1]) << std::endl;

            auto os = output.as_stream();
            hash_provider = new stackoverflow_hash();

            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE)
            {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            std::vector<std::unordered_map<long long, len_t>> map_storage;
            std::unordered_map<long long, hmap_value> hmap;
            std::vector<std::set<Cherry>::iterator> cherrylist;
            std::unordered_map<long long, len_t> temp_store;

            populate_buffer(WINDOW_SIZE, input_view);

            len_t ws = WINDOW_SIZE;
            std::cout << "ws:" << ws << ",MINFACTORLENGTH: " << MIN_FACTOR_LENGTH << std::endl;
            len_t round = 0;

            StatPhase::wrap("Init", [&] { std::cout << "0" << std::endl; });

            StatPhase::wrap("Factorization", [&] {
                while (ws > MIN_FACTOR_LENGTH)
                {

                    StatPhase::wrap("Round: " + std::to_string(round), [&] {
                        std::cout << FactorBuffer.size() << std::endl;
                        StatPhase::log("numbers of cherrys", FactorBuffer.size());
                        //std::cout << "0" << std::endl;
                        StatPhase::wrap("cherrylist: ", [&] { make_active_cherry_list(cherrylist); });
                        //std::cout << "1" << std::endl;
                        std::cout << "active: " << cherrylist.size() << std::endl;
                        StatPhase::log("numbers of active cherrys", cherrylist.size());
                        rhash = hash_provider->make_rolling_hash(0, ws / 2, input_view); //half because we test the children
                        //std::cout << "2" << std::endl;
                       
                        StatPhase::wrap("make multimap: ", [&] { populate_multimap(hmap, cherrylist, input_view); });
                        //std::cout << "3" << std::endl;
                        temp_store =  std::unordered_map<long long,len_t>();
                        StatPhase::wrap("mark cherrys: ", [&] { mark_cherrys(hmap, temp_store, input_view); });
                        //std::cout << "4" << std::endl;
                         hmap =  std::unordered_map<long long, hmap_value>();
                        StatPhase::wrap("store map: ", [&] {map_storage.push_back(temp_store);});
                        temp_store.clear();
                        StatPhase::wrap("apply findings: ", [&] { apply_findings(cherrylist); });
                        ws = ws / 2;
                        cherrylist.clear();
                        
                        round++;
                        std::cout<<"level done"<<std::endl;
                    });
                }
            });
            std::cout << "fac done." << std::endl;



            len_t last_pos = 0;
            len_t il = 0;


            std::vector<AproxFactor> facbuf;
            
            StatPhase::wrap("inflate chains: ", [&] {inflate_chains(facbuf,input_view);});
            FactorBuffer.clear();
            
            
             std::cout << "inflate done. facbuf size: "<<facbuf.size() << std::endl;
            StatPhase::wrap("insert src: ", [&] {insert_first_occ(facbuf, map_storage,input_view);});
            map_storage.clear();
            map_storage.shrink_to_fit();
             std::cout << "start encode." << std::endl;
            lzss::FactorBufferRAM factors;
            delete hash_provider;
            last_pos = 0;
            il = 0;
            for (AproxFactor f : facbuf)
            {
                il++;
                if (f.firstoccurence != f.position)
                {
                    if (f.position <= last_pos)
                    {
                        std::cout << "il: " << il << " last_pos: " << last_pos << "  pos: " << f.position << std::endl;
                        throw std::invalid_argument("not sorted");
                    }
                    factors.push_back(f.position, f.firstoccurence, f.length);
                    last_pos = f.position;
                }
            }


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
