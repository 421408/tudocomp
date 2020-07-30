#pragma once

#include <algorithm>
#include <functional>
#include <vector>

#include <tudocomp/Compressor.hpp>
#include <tudocomp/Range.hpp>
#include <tudocomp/Tags.hpp>
#include <tudocomp/util.hpp>

#include <tudocomp/decompressors/LZSSDecompressor.hpp>

#include <tudocomp/ds/DSManager.hpp>

#include <tudocomp_stat/StatPhase.hpp>

//custom

#include <unordered_map>
#include <string>
#include <tuple>
#include <iostream>


#include <tudocomp/def.hpp>
#include <tudocomp/util/rollinghash/rabinkarphash.hpp>

#include <tudocomp/compressors/lz77Aprox/AproxFactor.hpp>
#include <tudocomp/compressors/lz77Aprox/Cherry.hpp>

namespace tdc
{
    template <typename lzss_coder_t>
    class LZ77AproxCompressor : public Compressor
    {

    private:
        //this is ugly
        //used for hash
        const unsigned PRIME_BASE = 257;
        const unsigned PRIME_MOD = 1000000007;
        const len_t WINDOW_SIZE = 8;
        const len_t MIN_FACTOR_LENGTH = 1;

        //TODO is linked list better ? insert/spliting elem expensive here but fragmentation in list
        std::vector<Cherry> FactorBuffer;
        View input_view;

        //triple-tuple: hashvalue, position of first elem , size of hashwindow
        typedef std::tuple<long long, len_t, len_t> rolling_hash;

        //makes hash over part of the view
        long long hashfunction(len_t start, len_t size)
        {
            //https://stackoverflow.com/a/712275
            uint_fast64_t hash = 0;
            for (size_t i = 0; i < size; i++)
            {

                hash = hash * PRIME_BASE + input_view[start + i]; //shift and add
                hash %= PRIME_MOD;                                //don't overflow
            }
            return hash;
        }

        //takes a rollinghash triple over the view and advances it by one char
        void advance_rolling_hash(rolling_hash &hash)
        {
            //std::cout << "hash"<<std::endl;
            std::get<0>(hash) = std::get<0>(hash) * PRIME_BASE + input_view[std::get<1>(hash)+ std::get<2>(hash) + 1];        //shift hash & add new byte
            std::get<0>(hash) %= PRIME_MOD;                                                                                   //overflow
            std::get<0>(hash) -= input_view[std::get<1>(hash)] * std::pow(PRIME_BASE, std::get<2>(hash)); //remove first byte
            std::get<1>(hash) += 1;                                                                                           // advance position
        }

        //comfort function
        long long get_factor_hash(AproxFactor &fac){
        
            return hashfunction(fac.position, fac.length);
        }

        long long get_factor_hash_left(AproxFactor &fac){
            return hashfunction(fac.position, fac.length/2);
        }

        long long get_factor_hash_right(AproxFactor &fac){
            return hashfunction(fac.position + fac.length/2, fac.length/2);
        }

        //this function fills the FactorBuffer with the "first row" of the "tree"
        //TODO typedef FactorFlags cause they are ugly now (and unreadable)
        void populate_buffer(len_t window_size)
        {
            len_t position(0);
            //run until the next factor would reach beyond the file
            int end = input_view.size() - window_size;
            for (int i = 0; i <= end; i = i + window_size)
            {
                
                FactorBuffer.push_back(Cherry(position, window_size, 0));
                std::cout <<"Factor: "<< position <<std::endl;
                //cant use i after scope but must know pos for eof handle
                position = position+window_size;
            }
            
            //if the window size reaches beyond then split window until minSize
            len_t ws = window_size/2;
            while (ws > MIN_FACTOR_LENGTH){
                
                if(position+ws<=input_view.size()){
                    FactorBuffer.push_back(Cherry(position,ws,0));
                    std::cout <<"Factor: "<< position <<std::endl;
                    position=position+ws;
                    ws=ws/2;
                }else{
                    ws=ws/2;
                }
            }
            //marks the last factor as eof-factor if the MinFactor border lies beyond the end of the file
            //note that only the very last Factor in the Buffer can have flag 7 so only ever check the last factor for this case
            if(position<input_view.size()){
                FactorBuffer.push_back(Cherry(position,input_view.size()-position,7));
            }
            std::cout << "Populate Buffer done." <<std::endl;
        }


        //scans the FactorBuffer for all cherrys to be tested
        void make_active_cherry_list(std::vector<len_t> &vec,len_t size)
        {
            
            for (len_t iter = 0; iter < FactorBuffer.size()-1; iter++)
            {
                if (!FactorBuffer[iter].status && (size==FactorBuffer[iter].length)){

                        vec.push_back(len_t(iter));
                }
            }
        }

        //checks for true match between cherry and rooling hash
        //todo inline
        //returns true if they are truly equal AND HASH LIES BEFORE
        //false left check | true == right check
        bool false_positive_check_cherry_mem( bool right, len_t cherryindex,rolling_hash rhash){
            if(FactorBuffer[cherryindex].position>std::get<1>(rhash)+std::get<2>(rhash)){
                if(right){
                 return 0 == memcmp(&input_view[FactorBuffer[cherryindex].position + FactorBuffer[cherryindex].length/2],&input_view[std::get<1>(rhash)],FactorBuffer[cherryindex].length/2);
             }
                else{
                    return 0 == memcmp(&input_view[FactorBuffer[cherryindex].position] , &input_view[std::get<1>(rhash)], FactorBuffer[cherryindex].length/2);
             }
            }
            return false;
        }
        //if needed check 8byte at once not individualy
        bool false_positive_check_cherry(bool right, len_t cherryindex,rolling_hash rhash){
            if(FactorBuffer[cherryindex].position>std::get<1>(rhash)+std::get<2>(rhash)){
                len_t pos1;
                len_t pos2 = std::get<1>(rhash);
                //adjust start if left or right child
                if (right){
                    pos1 = FactorBuffer[cherryindex].position + FactorBuffer[cherryindex].length/2;
                }
                else{
                    pos1 = FactorBuffer[cherryindex].position;
                }
                //check ech byte if they are the same
                for(len_t index = 0; index<=std::get<2>(rhash);index++){
                    if(input_view[pos1+index]!=input_view[pos2+index]){
                        return false;
                    }
                }
                return true;
                
            }
            return false;
        }

        //searches with the rolling hash over the view and marks all children of the cherrys if found
        void mark_cherrys(std::unordered_multimap<long long,std::pair<bool, len_t>> &hmap,rolling_hash &rhash){

            //TODO lookup return type of equal_range
            auto maprange = hmap.equal_range(std::get<0>(rhash));
            
            for (len_t i = 0; i < input_view.size()-1-std::get<2>(rhash); i++)
            {
                //std::cout << i <<std::endl;
                //TODO ask range vs. bucket (bucket conatains potantialy non-same-key-elems)
                maprange = hmap.equal_range(std::get<0>(rhash));
                for (auto rangeiter = maprange.first; rangeiter != maprange.second; i++){
                
                    if(false_positive_check_cherry(rangeiter->second.first,rangeiter->second.second, rhash)){
                        //actually matching and not the same or just hashmatch
                        std::cout<< "Acually found:" << FactorBuffer[rangeiter->second.second].position << "at: " << std::get<2>(rhash) << std::endl;
                        if(rangeiter->second.first  ){
                            //right
                            FactorBuffer[rangeiter->second.second].rightfound=true;
                        }
                        else{
                            //left
                            FactorBuffer[rangeiter->second.second].leftfound=true;
                        }
                    }
                }

                advance_rolling_hash(rhash);
            }   
        }

        void populate_multimap(std::unordered_multimap<long long,std::pair<bool, std::vector<len_t>>> &hmap,std::vector<len_t> &cherrylist){
            for (len_t iter : cherrylist){
                
                hmap.insert({get_factor_hash_left(FactorBuffer[iter]),std::make_pair(false,int(iter))});
                hmap.insert({get_factor_hash_right(FactorBuffer[iter]), std::make_pair(true,int(iter))});
            }
        }

        void apply_findings(std::vector<len_t> &cherrylist){

             len_t insertoffset=0;
            std::vector<Cherry>::iterator iter;

            for(len_t index : cherrylist){
                //TODO i think this can be transformed into branchless
                //this is cancer
                
                if(FactorBuffer[index + insertoffset].rightfound&&FactorBuffer[index + insertoffset].leftfound){
                    //cherrydone
                    FactorBuffer[index + insertoffset].status=1;
                    std::cout << "cherrydone" << std::endl;
                }
                else{
                    if (!(FactorBuffer[index + insertoffset].rightfound)&& !(FactorBuffer[index + insertoffset].leftfound)){
                        //add new factor
                        //TODO SIZE is always the same no need for look up change
                        //TODO check if +1 is needed
                        iter = FactorBuffer.begin() + index + insertoffset +1;
                        FactorBuffer.insert(iter, FactorBuffer[index + insertoffset].split());
                        //iter is invalid here

                        //only increse after old factor has been changed
                        insertoffset++;
                        std::cout << "newfactor: " << FactorBuffer[index + insertoffset].position<< std::endl;
                    }
                    else{
                        if(FactorBuffer[index].leftfound){
                            //only leftfound
                            FactorBuffer[index + insertoffset].only_leftfound();
                        }
                        else{
                            //only rigth found
                            FactorBuffer[index + insertoffset].only_rightfound();
                        }
                    }
                   
                    
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
            m.param("threshold", "The minimum factor length.").primitive(2);
            return m;
        }

        using Compressor::Compressor;

        inline virtual void compress(Input &input, Output &output) override
        {

            input_view = input.as_view();

            std::unordered_multimap<long long,std::pair<bool, len_t>> hmap;
            std::vector<len_t> cherrylist;

            //std::cout << "1"<<std::endl;
            populate_buffer(WINDOW_SIZE);

            std::cout << std::endl;
            for (Cherry cher : FactorBuffer)
            {
                std::cout <<cher.position << ", ";
            }
            std::cout << std::endl;
            len_t ws = WINDOW_SIZE;

            while (ws>MIN_FACTOR_LENGTH){
            
                make_active_cherry_list(cherrylist,ws);
            
                rolling_hash rhash = std::make_tuple(hashfunction(0, ws), 0, ws);
            
                mark_cherrys(hmap,rhash);
       
                apply_findings(cherrylist);
                ws=ws/2;
                std::cout << "One level done"<<std::endl;
            }
            
        


            std::cout << std::endl;
            for (Cherry cher : FactorBuffer)
            {
                std::cout <<cher.position << ", ";
            }
            std::cout << std::endl;

        }

        inline std::unique_ptr<Decompressor> decompressor() const override
        {
            return Algorithm::instance<LZSSDecompressor<lzss_coder_t>>();
        }
    };

} // namespace tdc
