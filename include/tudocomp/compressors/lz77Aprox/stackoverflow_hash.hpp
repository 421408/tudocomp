#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{
namespace lz77Aprox{
    class  stackoverflow_hash final: public hash_interface{

    private:
        const unsigned PRIME_BASE;
        const int64_t PRIME_MOD;

    public:
        //constructor with good primes
        stackoverflow_hash():PRIME_BASE(257),PRIME_MOD((1ULL<<54)-33){}
        //constructor with custom primes
        stackoverflow_hash(uint64_t pb,uint64_t pm):PRIME_BASE(pb),PRIME_MOD(pm){}

        ~stackoverflow_hash(){};

        //returns hash of substring with given parameters
        inline uint64_t make_hash(len_t start, len_t size,io::InputView &input_view){


            long long hash =0;
            for (size_t i = 0; i < size; i++){
                hash = (((hash *PRIME_BASE)%PRIME_MOD)+input_view[start+i])%PRIME_MOD;
            }

            return hash;
        }
        //return a rollinghash struct defined above with position = start
        // and length = size
        inline rolling_hash make_rolling_hash(len_t start, len_t size,io::InputView &input_view){
            rolling_hash rhash;
            rhash.length=size;
            rhash.position=start;
            rhash.hashvalue=make_hash(start,size,input_view);

            rhash.c0_exp=1;
            for(len_t i=0;i<size-1;i++){
                rhash.c0_exp=(rhash.c0_exp*PRIME_BASE)%PRIME_MOD;
            }
            return rhash;
        }
        //advances the rollinghash struct by 1 and updates the hashvalue
        inline void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){
            int64_t oldhash = rhash.hashvalue;
            int64_t first= (input_view[rhash.position]*rhash.c0_exp)%PRIME_MOD;
            int64_t old_minus = (oldhash - first)%PRIME_MOD;
            int64_t d = (old_minus*PRIME_BASE)%PRIME_MOD;
            int64_t e = (d+input_view[rhash.position+rhash.length])%PRIME_MOD;
            if(e<0){
                e=e+PRIME_MOD;
            }
            rhash.hashvalue =e;
            rhash.position++;



      }



  };}//namespace lz77Aprox
   }//ns tdc