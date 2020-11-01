#pragma once

#include <tudocomp/def.hpp>


namespace tdc {

    class Chain {
    public:

        Chain(len_compact_t length, len_compact_t pos) {
            chain = 0;
            positon = pos;

        }

        Chain() {
            chain = 0;
        }

        uint16_t chain;
        len_compact_t positon;

        inline len_compact_t get_position() {
            return positon;
        }

        inline void set_position(len_compact_t pos) {
            positon = pos;

        }

        inline void add(len_compact_t &adder) {

            chain = chain + adder;
        }

        inline uint16_t get_chain() {
            return chain;
        }
    }__attribute__((__packed__));
}//ns