#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/hash_interface.hpp>

namespace tdc {
namespace lz77Aprox{
    class dna_nth_hash final : public hash_interface {

    //https://doi.org/10.1093/bioinformatics/btw397
    //explain:
    //https://bioinformatics.stackexchange.com/questions/19/are-there-any-rolling-hash-functions-that-can-hash-a-dna-sequence-and-its-revers
    public:

        dna_nth_hash() {}


        ~dna_nth_hash() {};
        //substitution table
        inline uint64_t shift(const uint8_t &byte) {
            switch (byte) {
                //A
                case 65:
                    return 4362857412768957556;
                    //C
                case 67:
                    return 3572411708064410444;
                    //G
                case 71:
                    return 2319985823310095140;
                    //T
                case 84:
                    return 2978368046464386134;
                    //N
                default:
                    return 0;

            }

            throw std::invalid_argument( "what" );
        }

        //returns rotation(cyclic shift) of val by exp
         inline uint64_t rotate(uint64_t val, uint64_t exp) {
            return val << exp | val >> (64 - exp);
        }

        //returns hash of substring with given parameters
        uint64_t make_hash(len_t start, len_t size, io::InputView &input_view) {


            uint64_t hash = 0;
            for (size_t i = 0; i < size; i++) {
                hash = rotate(hash, 1) ^ shift(input_view[start + i]);
            }

            return hash;
        }
        //return a rollinghash struct defined above with position = start
        // and length = size
        rolling_hash make_rolling_hash(len_t start, len_t size, io::InputView &input_view) {
            rolling_hash rhash;
            rhash.length = size;
            rhash.position = start;
            rhash.hashvalue = make_hash(start, size, input_view);

            rhash.c0_exp = 1;

            return rhash;
        }
        //advances the rollinghash struct by 1 and updates the hashvalue
        void advance_rolling_hash(rolling_hash &rhash, io::InputView &input_view) {


            rhash.hashvalue = rotate(rhash.hashvalue, 1) ^ rotate(shift(input_view[rhash.position]), rhash.length) ^
                              shift(input_view[rhash.position + rhash.length]);

            rhash.position++;



        }


    };
}//namespace lz77Aprox
}//ns tdc