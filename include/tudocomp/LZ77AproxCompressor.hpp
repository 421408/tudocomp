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
        struct hmap_value
        {
            bool right;
            len_t index;
            len_t first_occ;
            bool found;
        };

        //this is ugly
        len_t WINDOW_SIZE = 16;
        const len_t MIN_FACTOR_LENGTH = 1;

        std::vector<Cherry> FactorBuffer;
        View input_view;
        hash_interface *hash_provider;
        rolling_hash rhash;

        long long get_factor_hash(AproxFactor &fac) { return hash_provider->make_hash(fac.position, fac.length, input_view); }

        long long get_factor_hash_left(AproxFactor &fac) { return hash_provider->make_hash(fac.position, (fac.length / 2), input_view); }

        long long get_factor_hash_right(AproxFactor &fac) { return hash_provider->make_hash(fac.position + fac.length / 2, fac.length / 2, input_view); }

        //this function fills the FactorBuffer with the "first row" of the "tree"
        //TODO typedef FactorFlags cause they are ugly now (and unreadable)
        void populate_buffer(len_t window_size)
        {
            len_t position(0);
            //run until the next factor would reach beyond the file

            for (len_t i = 0; i <= input_view.size() - 1 - window_size; i = i + window_size)
            {

                FactorBuffer.push_back(Cherry(position, window_size, 0));

                //cant use i after scope but must know pos for eof handle
                //if you use position in loop last step will be missing and you dont know if it its exactly
                position = position + window_size;
            }

            //marks the last factor as eof-factor if the Factor border lies beyond the end of the file
            //note that only the very last Factor in the Buffer can have flag 7 so only ever check the last factor for this case
            if (position < input_view.size())
            {
                FactorBuffer.push_back(Cherry(position, window_size, 7));
            }
        }

        //scans the FactorBuffer for all cherrys to be tested
        void make_active_cherry_list(std::vector<len_t> &vec)
        {
            for (len_t iter = 0; iter < FactorBuffer.size(); iter++)
            {
                if ((FactorBuffer[iter].status == 0) || (FactorBuffer[iter].status == 7))
                {
                    vec.push_back(len_t(iter));
                }
            }
        }

        //checks for true match between cherry and rooling hash
        //todo inline
        //returns true if they are truly equal AND HASH LIES BEFORE
        //false left check | true == right check
        bool false_positive_check_cherry(bool right, len_t cherryindex)
        {
            if (right)
            {
                if ((FactorBuffer[cherryindex].position + FactorBuffer[cherryindex].length / 2) > rhash.position)
                {
                    return input_view.substr(FactorBuffer[cherryindex].position + FactorBuffer[cherryindex].length / 2, FactorBuffer[cherryindex].length / 2) == input_view.substr(rhash.position, rhash.length);
                }
                return false;
            }
            if (FactorBuffer[cherryindex].position > rhash.position)
            {
                return input_view.substr(FactorBuffer[cherryindex].position, FactorBuffer[cherryindex].length / 2) == input_view.substr(rhash.position, rhash.length);
            }
            return false;
        }

        //searches with the rolling hash over the view and marks all children of the cherrys if found
        void mark_cherrys(std::unordered_multimap<long long, hmap_value> &hmap)
        {

            //TODO lookup return type of equal_range
            auto maprange = hmap.equal_range(rhash.hashvalue);
            //dont let the hash rollbeyond the view
            for (len_t i = 0; i < input_view.size() - 1 - rhash.length; i++)
            {

                maprange = hmap.equal_range(rhash.hashvalue);

                for (auto rangeiter = maprange.first; rangeiter != maprange.second; rangeiter++)
                {

                    if (false_positive_check_cherry(rangeiter->second.right, rangeiter->second.index))
                    {
                        if(!rangeiter->second.found){
                            rangeiter->second.first_occ= rhash.position;
                            rangeiter->second.found=true;
                        }
                        
                        //actually matching and not the same or just hashmatch
                        if (rangeiter->second.right)
                        {
                            //right
                            FactorBuffer[rangeiter->second.index].rightfound = true;
                        }
                        else
                        {
                            //left
                            FactorBuffer[rangeiter->second.index].leftfound = true;
                        }
                    }
                }

                hash_provider->advance_rolling_hash(rhash);
            }
        }

        void populate_multimap(std::unordered_multimap<long long, hmap_value> &hmap, std::vector<len_t> &cherrylist)
        {
            for (len_t iter = 0; iter < cherrylist.size() - 1; iter++)
            {

                hmap.insert({get_factor_hash_left(FactorBuffer[iter]), hmap_value{false, len_t(iter)}});
                hmap.insert({get_factor_hash_right(FactorBuffer[iter]), hmap_value{true, len_t(iter)}});
            }
            //handle eof
            if (FactorBuffer[cherrylist.back()].status == 7)
            {
                len_t bull = FactorBuffer[cherrylist.back()].position;
                len_t shit = FactorBuffer[cherrylist.back()].length / 2;
                len_t bullshit = bull+shit;
                std::cout<< "popmap:"<<std::endl<<"size view: "<<input_view.size()<<",Cherrylegth: "<<bullshit<<std::endl;
                std::cout<<"bull:"<<bull<<"shit:"<<shit<<std::endl;
                if (bullshit >= input_view.size()-1)
                {
                    std::cout<< "shouldme"<<std::endl;
                    //leftchild also reaches Beyond
                    FactorBuffer[cherrylist.back()].split();
                    FactorBuffer[cherrylist.back()].status = 7;
                    cherrylist.pop_back();
                }
                else
                {
                    if (FactorBuffer[cherrylist.back()].position + FactorBuffer[cherrylist.back()].length / 2 < input_view.size()-1)
                    {
                        std::cout<< "not me"<<std::endl;
                        //only the right child reaches into the beyond
                        hmap.insert({get_factor_hash_left(FactorBuffer[cherrylist.back()]), hmap_value{false, cherrylist.back()}});
                    }
                }
            }
        }

        void apply_findings(std::vector<len_t> &cherrylist)
        {

            len_t insertoffset = 0;

            for (len_t index : cherrylist)
            {

                if (FactorBuffer[index + insertoffset].rightfound && FactorBuffer[index + insertoffset].leftfound)
                {
                    //cherrydone
                    FactorBuffer[index + insertoffset].status = 1;
                }
                else
                {
                    if (!(FactorBuffer[index + insertoffset].rightfound) && !(FactorBuffer[index + insertoffset].leftfound))
                    {
                        //neither left nor right found -> new cherry

                        FactorBuffer.insert(FactorBuffer.begin() + index + insertoffset + 1, FactorBuffer[index + insertoffset].split());

                        //only increse after old factor has been changed
                        insertoffset++;
                    }
                    else
                    {
                        if (FactorBuffer[index + insertoffset].leftfound)
                        {
                            //only leftfound
                            FactorBuffer[index + insertoffset].only_leftfound();
                        }
                        else
                        {
                            //only rigth found
                            FactorBuffer[index + insertoffset].only_rightfound();
                        }
                    }
                }
            }
        }


        //this is cancer
        len_t new_left_fac_pos(len_t position, len_t new_size, len_t orig_size) {

            while (new_size > orig_size) {
                position = position - new_size;
             new_size = new_size / 2;
         }
          return position;

        }

        len_t new_right_fac_pos(len_t position, len_t new_size, len_t orig_size){
            //adjust pos to end of cherry
            position=position+orig_size;
            while (new_size>orig_size){
                position=position+new_size;
                new_size=new_size/2;
            }
            return position;
        }

        void inflate_chains(std::vector<AproxFactor> &facbuf){
            len_t cherry_length;
            len_t curr_factor_length;
            for(Cherry c : FactorBuffer){

                curr_factor_length = std::pow(int(c.length),c.vectorL.size());
                for(bool b:c.vectorL){
                    if(b){
                        facbuf.push_back(AproxFactor(new_left_fac_pos(c.position,curr_factor_length,c.length),curr_factor_length,2));
                    }
                    curr_factor_length=curr_factor_length/2;
                }

                facbuf.push_back(AproxFactor(c.position,c.length/2,2));
                facbuf.push_back(AproxFactor(c.position+c.length/2,c.length/2,2));

                curr_factor_length = std::pow(int(c.length),c.vectorR.size());
                for(bool b:c.vectorR){
                    if(b){
                        facbuf.push_back(AproxFactor(new_right_fac_pos(c.position,curr_factor_length,c.length),curr_factor_length,2));
                    }
                }

            }
        }

        len_t tree_level_by_size( len_t size){
            len_t i=0;
            len_t ws = WINDOW_SIZE;
            while(ws!=size){
                ws=ws/2;
                i++;
            }
            return i;
        }

        void insert_first_occ(std::vector<AproxFactor> &facbuf,std::vector<std::unordered_multimap<long long, hmap_value>> &map_storage){
            
            for(AproxFactor c :facbuf){
               //get the right bucket
               auto map_range= map_storage[tree_level_by_size(c.length)].equal_range(get_factor_hash_left(c));
                    for (auto range_iter = map_range.first; range_iter != map_range.second; range_iter++) {
                        c.firstoccurence=range_iter->second.first_occ;
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
            m.param("window", "The sliding window size").primitive(16);
            m.param("threshold", "The minimum factor length.").primitive(2);
            m.inherit_tag<lzss_coder_t>(tags::lossy);
            return m;
        }

        using Compressor::Compressor;

        inline virtual void compress(Input &input, Output &output) override
        {
            
            input_view = input.as_view();
            auto os = output.as_stream();
            hash_provider = new stackoverflow_hash();
            std::string ba ="abracadabracaabracadabracazz";


            //adjust window size
            while (input_view.size() - 1 < WINDOW_SIZE)
            {
                WINDOW_SIZE = WINDOW_SIZE / 2;
            }

            std::vector<std::unordered_multimap<long long, hmap_value>> map_storage;
            std::unordered_multimap<long long, hmap_value> hmap;
            std::vector<len_t> cherrylist;

            populate_buffer(WINDOW_SIZE);

            len_t ws = WINDOW_SIZE;

            while (ws > MIN_FACTOR_LENGTH)
            {

                make_active_cherry_list(cherrylist);
                for(len_t t:cherrylist){
                    std::cout<< t <<",";
                }
                std::cout<<std::endl;
                rhash = hash_provider->make_rolling_hash(0, ws / 2, input_view); //half because we test the children

                populate_multimap(hmap, cherrylist);
                for(len_t t:cherrylist){
                    std::cout<< t <<",";
                }
                std::cout<<std::endl;
                mark_cherrys(hmap);

                apply_findings(cherrylist);
                ws = ws / 2;

                cherrylist.clear();
                map_storage.push_back(hmap);
                hmap.clear();

            }
            std::vector<AproxFactor> facbuf;

            inflate_chains(facbuf);
            insert_first_occ(facbuf,map_storage);

            // initialize encoder
            // auto coder = lzss_coder_t(config().sub_config("coder"))
            // .encoder(output, NoLiterals());
            // coder.encode_header();

            // for(AproxFactor f:facbuf){

            //     if(f.position!=f.firstoccurence){
            //         coder.encode_factor(lzss::Factor(f.position,f.firstoccurence,f.length));
            //     }
            //     else{
            //         for(int i =0;i<f.length;i++){
            //             coder.encode_factor(lzss::Factor(f.position,0,f.length));
            //         }
            //     }
            // }
            std::cout <<"blu"<<std::endl;
            lzss::FactorBufferRAM factors;
            for(AproxFactor f: facbuf){
                if(f.firstoccurence!=f.position){
                    factors.push_back(f.position,f.firstoccurence,f.length);
                }
            }
            std::cout <<"bla"<<std::endl;
            // encode
            StatPhase::wrap("Encode", [&]{
            auto coder = lzss_coder_t(config().sub_config("coder")).encoder(
                output, lzss::UnreplacedLiterals<decltype(input_view), decltype(factors)>(input_view, factors));

            coder.encode_text(input_view, factors);
            });


        }

        inline std::unique_ptr<Decompressor> decompressor() const override
        {
            return Algorithm::instance<LZSSDecompressor<lzss_coder_t>>();
        }
    };

} // namespace tdc
