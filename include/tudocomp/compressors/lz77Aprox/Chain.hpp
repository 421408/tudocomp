#pragma once

#include <tudocomp/def.hpp>


namespace tdc {
    namespace lz77Aprox {
        class Chain {
            //this class is used to represent a node and its "bitvector"
            // on the "simulated" binarytree
            // it contains no special functions

        public:
            //constructor takes length and start position
            Chain(len_compact_t length, len_compact_t pos) {
                chain = 0;
                positon = pos;

            }

            Chain() {
                chain = 0;
            }

            //encodes the found/unfound factors
            uint32_t chain;

            len_compact_t positon;

            //getter and setter
            inline len_compact_t get_position() {
                return positon;
            }

            inline void set_position(len_compact_t pos) {
                positon = pos;

            }

            inline uint16_t get_chain() {
                return chain;
            }

            //function to mark found factors
            // since all factors fit in uint16_t
            //it is just and add (or and)
            inline void add(len_compact_t &adder) {

                chain = chain + adder;
            }


        }__attribute__((__packed__));
    }//namespace lz77Aprox
}//ns