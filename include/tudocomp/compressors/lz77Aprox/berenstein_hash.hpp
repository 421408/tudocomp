#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{
namespace lz77Aprox{
    //bernsteinhash djb2 dan Bernstein
    class   berenstein_hash final : public hash_interface{

    private:
        const uint32_t PRIME_BASE=33;
        const int64_t PRIME_MOD=(1ULL<<31)-1;

    public:

        berenstein_hash(){}



        ~berenstein_hash(){};

        //returns hash of substring with given parameters
        uint32_t make_hash(len_t start, len_t size,io::InputView &input_view){


            long long hash =0;
            for (size_t i = 0; i < size; i++){
                hash = modulo(modulo(modulo(hash<<5)+hash) +input_view[start+i]);
            }

            return hash;
        }
        //return a rollinghash struct defined above with position = start
        // and length = size
        rolling_hash make_rolling_hash(len_t start, len_t size,io::InputView &input_view){
            rolling_hash rhash;
            rhash.length=size;
            rhash.position=start;
            rhash.hashvalue=make_hash(start,size,input_view);

            rhash.c0_exp=1;
            for(len_t i=0;i<size-1;i++){
                rhash.c0_exp=modulo(rhash.c0_exp*PRIME_BASE);
            }
            return rhash;
        }

        //fast modulo with mersenne primes
        //https://ariya.io/2007/02/modulus-with-mersenne-prime
        inline uint32_t modulo(uint32_t hash){
            uint32_t moded = (hash & PRIME_MOD)+(hash>>31);
            return (moded>=PRIME_MOD) ? hash - PRIME_MOD : moded;
        }
        //advances the rollinghash struct by 1 and updates the hashvalue
        void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){

            uint32_t oldhash=rhash.hashvalue;

            uint32_t first = modulo(input_view[rhash.position]*rhash.c0_exp);

            uint32_t old_minus = modulo(oldhash-first);
            uint32_t c = modulo(old_minus<<5);
            uint32_t d = modulo(c+old_minus);

            rhash.hashvalue = modulo(d+input_view[rhash.position+rhash.length]);

            rhash.position++;


        }



    };
}//namespace lz77Aprox
}//ns tdc