#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>
namespace tdc{

class stackoverflow_hash : public hash_interface{

    private:
        const unsigned PRIME_BASE;
        const unsigned PRIME_MOD;

    public:

    stackoverflow_hash():PRIME_BASE((1ULL<<15)-19),PRIME_MOD(1000000009){}

    stackoverflow_hash(uint64_t pb,uint64_t pm):PRIME_BASE(pb),PRIME_MOD(pm){}

    ~stackoverflow_hash(){};


    long long make_hash(len_t start, len_t size,io::InputView &input_view){

        
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
        
        rhash.c0_exp=1;
        for(len_t i=0;i<size-1;i++){
            rhash.c0_exp=(rhash.c0_exp*PRIME_BASE)%PRIME_MOD;
        }
        return rhash;
    }

    void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view){
        rhash.hashvalue = (((rhash.hashvalue-((input_view[rhash.position]*rhash.c0_exp)%PRIME_MOD))*PRIME_BASE)%PRIME_MOD+input_view[rhash.position+rhash.length])%PRIME_MOD;
        if(rhash.hashvalue<0){
            rhash.hashvalue=rhash.hashvalue+PRIME_MOD;
        }
        rhash.position++;
          //if(rhash.hashvalue!=make_hash(rhash.position,rhash.length,input_view)){
          //    std::cout<<input_view.substr(rhash.position,rhash.length)<<std::endl;
          //  std::cout<<"rhash: "<<rhash.hashvalue<<" ,mhash: "<<make_hash(rhash.position,rhash.length,input_view)<<std::endl;
          //   std::cout<<"exp: "<<rhash.c0_exp*input_view[rhash.position-1]<<std::endl;
            //  std::cout<<"exp*c0: "<<(rhash.c0_exp*input_view[rhash.position-1])%PRIME_MOD<<std::endl;
            //  std::cout<<"pos: "<<rhash.position<<" l: "<<rhash.length<<std::endl;
           //  std::cout<<"int: "<<int(input_view[rhash.position+rhash.length-1])<<" int(char): "<<int(char(input_view[rhash.position+rhash.length-1]))<<std::endl;
            //  std::cout<<"ascii: "<<input_view[rhash.position-1]<<" int: "<<int(input_view[rhash.position-1])<<std::endl;

           //  throw std::invalid_argument( "doesnt match hash" );
         // }
        

    }



};}