#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/util.hpp>

namespace tdc{

    struct rolling_hash
    {
        len_t position,length;
        long long hashvalue;
        long long c0_exp;
    };
    

    class hash_interface{
        public:

        
        virtual ~hash_interface(){};

        virtual long long make_hash(len_t start, len_t size,io::InputView &input_view) =0;

        virtual rolling_hash make_rolling_hash(len_t start,len_t size,io::InputView &input_view)=0;

        virtual void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view) =0;
    };
    }//ns