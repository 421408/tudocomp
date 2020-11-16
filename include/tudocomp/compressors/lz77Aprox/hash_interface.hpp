#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/util.hpp>

namespace tdc{
    //struct for search
    struct rolling_hash
    {
        len_t position,length;
        //holds the hashvalue of substring beginning at position
        // with length length
        uint64_t hashvalue;

        //holds the multiplier of the value we remove for
        //polynomal hashes
        uint64_t c0_exp;

    };
    

    class hash_interface{
        public:

        //destructor
        virtual ~hash_interface(){};

        //returns hash of substring with given parameters
        virtual uint64_t make_hash(len_t start, len_t size,io::InputView &input_view) =0;

        //return a rollinghash struct defined above with position = start
        // and length = size
        virtual rolling_hash make_rolling_hash(len_t start,len_t size,io::InputView &input_view)=0;

        //advances the rollinghash struct by 1 and updates the hashvalue
        virtual void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view) =0;
    };
    }//ns