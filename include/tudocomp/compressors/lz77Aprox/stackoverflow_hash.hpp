#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{

class stackoverflow_hash : public hash_interface{

    private:
        const unsigned PRIME_BASE;
        const unsigned PRIME_MOD;

    public:

    stackoverflow_hash():PRIME_BASE(137),PRIME_MOD(1000000009){}

    stackoverflow_hash(uint64_t pb,uint64_t pm):PRIME_BASE(pb),PRIME_MOD(pm){}

    ~stackoverflow_hash(){};


    long long make_hash(len_t start, len_t size,io::InputView &input_view){

        //https://stackoverflow.com/a/712275
         long long hash =0;
        for (size_t i = 0; i < size; i++){
            hash = (((hash *PRIME_BASE)%PRIME_MOD)+input_view[start+i])%PRIME_MOD;
        }
       
        return hash;
    }

    rolling_hash make_rolling_hash(len_t start, len_t size,io::InputView &input_view){
        rolling_hash rhash;
        rhash.length=size;
        rhash.position=start;
        rhash.hashvalue=make_hash(start,size,input_view);
        rhash.input_view=input_view;
        rhash.c0_exp=1;
        for(len_t i=0;i<size-1;i++){
            rhash.c0_exp=(rhash.c0_exp*PRIME_BASE)%PRIME_MOD;
        }
        return rhash;
    }

    void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){
        long long old = rhash.hashvalue;
        char c0 = rhash.input_view[rhash.position];
        char cn = rhash.input_view[rhash.position+rhash.length];
        rhash.hashvalue = (((old-((c0*rhash.c0_exp)%PRIME_MOD))*PRIME_BASE)%PRIME_MOD+cn)%PRIME_MOD;
        if(rhash.hashvalue<0){
            rhash.hashvalue=rhash.hashvalue+1000000009;
        }

        rhash.position++;

    }



};}