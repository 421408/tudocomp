#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/util.hpp>

namespace tdc{

    struct rolling_hash
    {
        len_t position,length;
        int64_t hashvalue;
        uint64_t c0_exp;
    };
    

    class hash_interface{
        public:

        
        virtual ~hash_interface(){};

        virtual uint64_t make_hash(len_t start, len_t size,io::InputView &input_view) =0;

        virtual rolling_hash make_rolling_hash(len_t start,len_t size,io::InputView &input_view)=0;

        virtual void advance_rolling_hash(rolling_hash &rhash,io::InputView &input_view) =0;
    };
    }//ns