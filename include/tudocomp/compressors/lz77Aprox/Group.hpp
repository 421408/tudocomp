#pragma once

#include <tudocomp/def.hpp>
#include <tudocomp/compressors/lz77Aprox/Chain.hpp>
#include <tudocomp/compressors/lz77Aprox/Factor.hpp>

namespace tdc
{

    class Group
    {
    public:
        Group(Chain c, bool inc,uint16_t last_size_checked)
        {
            tookpartinlast =false;
            position = c.get_position();
             length = 1 << (__builtin_ctz(c.get_chain()));

            bool last_set = c.get_chain() &last_size_checked;
            if(!last_set&&inc){
                position=position+last_size_checked;
            }
            if(!last_set&&(!inc)){
                position=position-1;
            }
            if(last_set&&(!inc)){
                position = position+length-1;
            }
            

           
            chain = c.get_chain() - length;
            increasing = inc;

            if(increasing){
                src_position = position;
            }
            else{
                src_position = position-length+1;
            }



            if (position==140120943){

                std::cout<<"tada\n";
            }
            
            first = true;

        }

        uint16_t chain;
        len_t position;
        len_t src_position;
        uint16_t length;
        bool increasing;
        bool first;
        bool tookpartinlast;

        
        inline len_compact_t get_position()
        {
            return position;
        }
        inline void set_position(len_t pos)
        {
            position = pos;
        }

        inline len_t get_src_position()
        {
            return src_position;
        }
        inline void set_src_position(len_t pos)
        {
            src_position = pos;
        }

        inline uint16_t get_length()
        {
            return length;
        }

        inline uint16_t get_next_length()
        {
            if (chain==0){

                std::cout<<"tada\n";
            }
            return 1 << (__builtin_ctz(chain));
        }
        inline len_t get_start_of_search()
        {
            if (chain==0){

                std::cout<<"   ";
            }
            
            if (increasing)
            {
                return position;
            }
            else
            {
                return position+ 1 - (get_next_length() * 2);
            }
        }

        inline bool has_next()
        {
            if (position==140120943){

                std::cout<<"  ";
            }
            return chain;
        }

        inline void absorp_next(len_t src)
        {
            if (position==140120943){
                std::cout<<" ";
            }
            if (chain==0){

                std::cout<<"  ";
            }
            if (position==172574063){

                std::cout<<" ";
            }
            len_t size = 1 << (__builtin_ctz(chain));
            chain = chain - size;

          
            if(increasing){
                src_position = src;
            }
            else{
                src_position = src+(size-length);
            }

            length = length + size;
            if (position==140120943){

                std::cout<<"  ";
            }
            if (position==172574063){

                std::cout<<" ";
            }

        }

        inline lz77Aprox::Factor advance_Group()
        {
            

            len_t old_position = position;
            len_t old_src_position = src_position;
            len_t old_length = length;

            if (chain==0){

                std::cout<<"  ";
            }
            if (position==172574063){

                std::cout<<" ";
            }

            if (position==73738351){

                std::cout<<"  ";
            }

            len_t size = 1 << (__builtin_ctz(chain));

            if (increasing)
            {
                position = position + length;
                chain = chain - size;
                length = size;
                src_position = position;



                if (first)
                {
                    first = false;
                    return lz77Aprox::Factor(old_position, old_src_position, old_length, true);
                }
                else
                {
                    return lz77Aprox::Factor(old_position, old_src_position, old_length,true);
                }
            }
            else
            {
                //decreasing
                position = position - length;
                chain = chain - size;
                length = size;
                src_position = position-length+1;
                if (position==172574063){

                    std::cout<<" ";
                }

                if (first)
                {
                    first = false;

                    return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length, true);
                }
                else
                {

                    return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length, true);
                }
            }




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
                return length + next_length * 2;
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