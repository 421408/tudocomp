#pragma once
#include <tudocomp/def.hpp>

namespace tdc{
    class AproxFactor{

    public:
        /*  0= done
         *  1= cherry
         *  2= chain up
         *  3= chain down
         *  4= 1 time optimised
         *  5= 2 time optimised
         *  6= 3 time optimised(done)
         *  7= eof factor
         *  TODO: maybe check type of class for fewer stati(statuses?)
         */
        mutable unsigned int status : 3;
        
        len_compact_t position;

        mutable len_compact_t length, firstoccurence;
        /*TODO:
         * 1: position could be divided by min FactorLength to shorten it.
         * 2: length is always a power of 2 between maxWindowLength and minFactorLength
         * length could be stored as the depth of the tree which would only be
         *  ceiling(log2( log2(|W|) - log2(|minFactor|) ) ) bits.
         *  This number is small if len_compact resizes mallocs can be inefficient.
         *  Also changing size could be done as a simple bit shift if not compressed
         * */

        inline AproxFactor() {
            // undefined!
        }


        inline AproxFactor(len_t pos,len_t len,unsigned int flag):position(pos){
            length= len;
            status= flag;
            firstoccurence=position;
        }
            bool operator< (AproxFactor x  )const{
        return this->position< x.position;
    }
    }__attribute__((__packed__));
}