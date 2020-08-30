#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/util.hpp>

namespace tdc{

    struct rolling_hash
    {
        len_t position,length;
        long long hashvalue;
        View input_view;
    };
    

    class hash_interface{
        public:

        
        virtual ~hash_interface(){};

        virtual long long make_hash(len_t start, len_t size,View &inputview) =0;

        virtual rolling_hash make_rolling_hash(len_t start,len_t size,View &inputview)=0;

        virtual void advance_rolling_hash(rolling_hash &rhash) =0;
    };
    }//ns