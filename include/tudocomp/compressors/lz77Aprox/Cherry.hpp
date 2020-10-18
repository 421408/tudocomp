#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/AproxFactor.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <vector>
#include <utility>

namespace tdc{

class Cherry {
public:
     /*  Left and right Vector which holds information about the chains
     *  of the cherry
     */
    mutable long long l :64;
    mutable long long r :64;

    mutable unsigned int status : 3;
        
    len_compact_t position;
        
    mutable len_compact_t length;


    //which children are found
   mutable bool leftfound, rightfound;
    Cherry(len_t pos, len_t len, len_t fl){
        position = pos;
        length=len;
        status = fl;

        l=0;
        r=0;
        leftfound=false;
        rightfound=false;
    }

    inline Cherry(){
        //undefined!
    }
    
    //return the rightchild and turns this into leftchild
    // this is const cause status and length are mutable
    inline Cherry split() const {
        Cherry rightchild = Cherry(position + length/2,length/2,0);
        rightchild.r=r;
        r=0;

        rightchild.status=status;
        //change this

        length=length/2;
        status=0;
        //return

        return rightchild;
    }
    //this is the only non-const func cause we change the position
    inline void only_leftfound(){
        //add a 1 at the right pos
        l=l+length/2;
        position = position +length/2;
        
        length=length/2;

        reset_found();
    }
    inline void only_rightfound() const {
        r=r+length/2;
        length=length/2;
        reset_found();

    }
    inline void reset_found()const{
        rightfound=false;
        leftfound=false;

    }

    inline std::pair<Chain,Chain> split_into_chains(){
        
        return std::make_pair(Chain(position,length/2,3,l),Chain(position+length/2,length/2,2,l)) ;

    }

    bool operator< (Cherry x  )const{
        return this->position< x.position;
    }

    bool operator== (Cherry x)const{
        return this->position==x.position;
    }
}__attribute__((__packed__));

}