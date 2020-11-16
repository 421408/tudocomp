#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <tudocomp/compressors/lz77Aprox/Factor.hpp>

namespace tdc {
    namespace lz77Aprox {

        class Group {
            //this class is used to represent a chain with its active group/factor
            //it implements methods to absorb the next factor
            // or to output the active group/factor

        public:
            //Constructor
            //Takes a chain object , a bool (increasing/decreasing)
            //a bitmask of the last size that was checked
            Group(Chain c, bool inc, uint16_t last_size_checked) {
                //set Position
                position = c.get_position();

                //length of active group/factor is the last set bit
                length = 1 << (__builtin_ctz(c.get_chain()));

                //check if the chain came from a true cherry or a normal chain
                bool last_set = c.get_chain() & last_size_checked;

                //set the position of the active group/factor
                //for increasing the position denotes the start of the active group/factor
                //for decreasing the posizion is the end of the active group/factor
                if (!last_set && inc) {
                    position = position + last_size_checked;
                }
                if (!last_set && (!inc)) {
                    position = position - 1;
                }
                if (last_set && (!inc)) {
                    position = position + length - 1;
                }

                //the other factors are encoded in the chain minus the active group/factor
                chain = c.get_chain() - length;
                increasing = inc;

                //set src_position
                if (increasing) {
                    src_position = position;
                } else {
                    src_position = position - length + 1;
                }


            }

            //"QUEUE" that encodes the remaining factors
            uint32_t chain;


            bool increasing;

            //active group/factor
            len_t position;
            len_t src_position;
            uint32_t length;

            //getter and setter
            inline len_compact_t get_position() {
                return position;
            }

            inline void set_position(len_t pos) {
                position = pos;
            }

            inline len_t get_src_position() {
                return src_position;
            }

            inline void set_src_position(len_t pos) {
                src_position = pos;
            }

            inline uint16_t get_length() {
                return length;
            }


            inline uint16_t get_chain() {
                return chain;
            }

            //return the length of the next factor which we want to merge
            inline uint16_t get_next_length() {

                return 1 << (__builtin_ctz(chain));
            }

            //returns the start position of the searchstring
            //which has 2* length of next factor
            inline len_t get_start_of_search() {


                if (increasing) {
                    return position;
                } else {
                    return position + 1 - (get_next_length() * 2);
                }
            }

            // if the chain is non null a next factor exists
            inline bool has_next() {

                return chain;
            }

            // function to merge the next factor into the active group/factor
            inline void absorp_next(len_t src) {

                // get size of next factor
                len_t size = 1 << (__builtin_ctz(chain));
                //remove next factor from the chain
                chain = chain - size;

                //adjust src_position
                if (increasing) {
                    src_position = src;
                } else {
                    src_position = src + (size - length);
                }

                // adjust length
                length = length + size;


            }

            // function to output the active group/factor
            // and make the next factor the active group/factor
            inline lz77Aprox::Factor advance_Group() {


                len_t old_position = position;
                len_t old_src_position = src_position;
                len_t old_length = length;

                // get size of next factor
                len_t size = 1 << (__builtin_ctz(chain));

                if (increasing) {
                    // if we are an increasing chain the values are pretty clear
                    position = position + length;
                    chain = chain - size;
                    length = size;
                    src_position = position;


                    return lz77Aprox::Factor(old_position, old_src_position, old_length);

                } else {
                    //if we are a decreasing chain
                    //adjust position from end to start
                    position = position - length;
                    chain = chain - size;
                    length = size;
                    src_position = position - length + 1;


                    return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length);

                }


            }


        } __attribute__((__packed__));
    }//namespace lz77Aprox
} // namespace tdc