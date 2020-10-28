#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{

    //bernsteinhash djb2 dan Bernstein
    class berenstein_hash : public hash_interface{

    private:
        const unsigned PRIME_BASE=33;
        const int64_t PRIME_MOD=(1ULL<<31)-1;

    public:

        berenstein_hash(){}



        ~berenstein_hash(){};


        long long make_hash(len_t start, len_t size,io::InputView &input_view){


            long long hash =0;
            for (size_t i = 0; i < size; i++){
                hash = modulo(modulo(modulo(hash<<5)+hash) +input_view[start+i]);
            }

            return hash;
        }

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
        //https://ariya.io/2007/02/modulus-with-mersenne-prime
        inline int64_t modulo(int64_t hash){
            int64_t moded = (hash & PRIME_MOD)+(hash>>31);
            return (moded>=PRIME_MOD) ? hash - PRIME_MOD : moded;
        }

        void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){

            int64_t oldhash=rhash.hashvalue;
            int64_t hash_minus_first = modulo(oldhash-modulo(input_view[rhash.position]*rhash.c0_exp));
            int64_t hash_minus_first_t = (oldhash-((input_view[rhash.position]*rhash.c0_exp)%PRIME_MOD))%PRIME_MOD;
            uint64_t fchar = input_view[rhash.position]*rhash.c0_exp;
            uint64_t first = modulo(input_view[rhash.position]*rhash.c0_exp);
            uint64_t fcharmod = (input_view[rhash.position]*rhash.c0_exp)%PRIME_MOD;
            uint64_t old_minus = modulo(oldhash-first);
            uint64_t c = modulo(old_minus<<5);
            uint64_t d = modulo(c+old_minus);
            uint64_t e = modulo(d+input_view[rhash.position+rhash.length]);

            rhash.hashvalue = modulo(modulo(modulo(hash_minus_first<<5)+hash_minus_first)  +input_view[rhash.position+rhash.length]);




            if(rhash.hashvalue<0){
                std::cout<<"rhash: "<<rhash.hashvalue<<" ,mhash: "<<make_hash(rhash.position,rhash.length,input_view)<<std::endl;
                rhash.hashvalue=rhash.hashvalue+PRIME_MOD;
                std::cout<<"rhash: "<<rhash.hashvalue<<" ,mhash: "<<make_hash(rhash.position,rhash.length,input_view)<<std::endl;
            }
            rhash.position++;

             if(rhash.hashvalue!=make_hash(rhash.position,rhash.length,input_view)){
                long long hash =0;
                 for (size_t i = 0; i < rhash.length; i++){
                     uint64_t x=modulo(hash<<5);
                     uint64_t y=modulo(x+hash);

                     hash = modulo( y+input_view[rhash.position+i]);
                 }


                std::cout<<input_view.substr(rhash.position,rhash.length)<<std::endl;
              std::cout<<"rhash: "<<rhash.hashvalue<<" ,mhash: "<<make_hash(rhash.position,rhash.length,input_view)<<std::endl;
               std::cout<<"exp: "<<rhash.c0_exp*input_view[rhash.position-1]<<std::endl;
                std::cout<<"exp*c0: "<<(rhash.c0_exp*input_view[rhash.position-1])%PRIME_MOD<<std::endl;
                std::cout<<"pos: "<<rhash.position<<" l: "<<rhash.length<<std::endl;
               std::cout<<"int: "<<int(input_view[rhash.position+rhash.length-1])<<" int(char): "<<int(char(input_view[rhash.position+rhash.length-1]))<<std::endl;
                std::cout<<"ascii: "<<input_view[rhash.position-1]<<" int: "<<int(input_view[rhash.position-1])<<std::endl;

              throw std::invalid_argument( "doesnt match hash bern" );
            }


        }



    };}