#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{

class stackoverflow_hash : public hash_interface{

    private:
        const unsigned PRIME_BASE;
        const unsigned PRIME_MOD;

    public:

    stackoverflow_hash():PRIME_BASE(257),PRIME_MOD(1000000007){}

    stackoverflow_hash(uint64_t pb,uint64_t pm):PRIME_BASE(pb),PRIME_MOD(pm){}

    long long make_hash(len_t start, len_t size,View &input_view){

        //https://stackoverflow.com/a/712275
        uint_fast64_t hash = 0;
        for (size_t i = 0; i < size; i++){

            hash = hash * PRIME_BASE + input_view[start + i]; //shift and add
            //hash %= PRIME_MOD;                                //don't overflow
        }
        return hash;
    }

    rolling_hash make_rolling_hash(len_t start, len_t size,View &input_view){
        rolling_hash rhash;
        rhash.length=size;
        rhash.position=start;
        rhash.hashvalue=make_hash(start,size,input_view);
        rhash.input_view=input_view;
        return rhash;
    }

    void advance_rolling_hash(rolling_hash &rhash){
        rhash.hashvalue=rhash.hashvalue-rhash.input_view[rhash.position]*std::pow(PRIME_BASE,rhash.length-1);//remove first char
        rhash.hashvalue=rhash.hashvalue*PRIME_BASE;//multiply all chars by base
        rhash.hashvalue=rhash.hashvalue+rhash.input_view[rhash.position+rhash.length];//add new char
        
        rhash.position++;

        //rhash.hashvalue=make_hash(rhash.position,rhash.length,rhash.input_view);
    }



};}