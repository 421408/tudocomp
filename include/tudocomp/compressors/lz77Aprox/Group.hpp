#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <tudocomp/compressors/lz77Aprox/Factor.hpp>

namespace tdc
{

    class Group
    {
    public:
        Group(Chain c, bool inc)
        {

            position = c.get_position();
            length = 1 << (__builtin_ctz(c.get_chain()));
            chain = c.get_chain() - length;
            src_position = c.get_position();
            increasing = inc;
            // if(c.get_chain()>=(4096*2)-1){
            //     std::cout<<"cain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group1" );
            // }
            // if(61440<=chain){
            //     std::cout<<"size to large chain: "<<chain<<" length "<<length<<std::endl;
            //     std::cout<<"c.chain: "<<c.get_chain()<<" length "<<c.get_position()<<std::endl;
            //     throw std::invalid_argument( "chain to large group41" );
            // }
            // if(61440==chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group2" );
            // }
        }

        uint16_t chain;
        len_compact_t position;
        len_compact_t src_position;
        uint16_t length;
        bool increasing;

        inline len_compact_t get_position()
        {
            return position;
        }
        inline void set_position(len_compact_t pos)
        {
            position = pos;
        }

        inline len_compact_t get_src_position()
        {
            return src_position;
        }
        inline void set_src_position(len_compact_t pos)
        {
            src_position = pos;
        }

        inline uint16_t get_length()
        {
            return length;
        }

        inline uint16_t get_next_length()
        {
            //std::cout<<chain<<std::endl;
            return 1 << (__builtin_ctz(chain));
        }
        inline len_compact_t get_start_of_search()
        {
            if (increasing)
            {
                return position;
            }
            else
            {
                return position - get_next_length() * 2;
            }
        }

        inline bool has_next()
        {
            return chain;
        }

        inline void absorp_next(len_compact_t src)
        {
            len_t size = 1 << (__builtin_ctz(chain));
            chain = chain - size;

            length = length + size;
            src_position = src;

            // if(size>chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group3" );
            // }
            // if(61440<=chain){
            //     std::cout<<"size to large chain: "<<chain<<" size: "<<size<<std::endl;
            //     throw std::invalid_argument( "chain to large group4" );
            // }

            // if(61440==chain){
            //     std::cout<<"size to large chain: "<<chain<<" size: "<<size<<std::endl;
            //     throw std::invalid_argument( "chain to large group5" );
            // }
        }

        inline lz77Aprox::Factor advance_Group()
        {
            len_compact_t old_position = position;
            len_compact_t old_src_position = src_position;
            len_compact_t old_length = length;

            len_compact_t size = 1 << (__builtin_ctz(chain));
            // if(size>chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group6" );
            // }
            // if(61440==chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group7" );
            // }
            if (increasing)
            {
                position = position + length;
                chain = chain - size;
                length = size;
                src_position = position;
                return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length);
            }
            else
            {
                position = position - length;
                chain = chain - size;
                length = size;
                src_position = position;
                return lz77Aprox::Factor(old_position, old_src_position, old_length);
            }
            // if(61440==chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group8" );
            // }
        }
        uint16_t get_max_search_length()
        {
            uint16_t next_length = get_next_length();

            if (__builtin_popcount(chain) >= 2)
            {
                uint16_t next_next_length = 1 << (__builtin_ctz(chain - next_length));
                return length + next_length + next_next_length;
            }
            else
            {
                return length + next_length*2;
            }
        }

        uint16_t get_min_search_length()
        {
            return length + get_next_length() + 1;
        }

        inline uint16_t get_chain()
        {
            return chain;
        }
    } __attribute__((__packed__));
} // namespace tdc