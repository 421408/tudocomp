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
            last_size=last_size_checked;
            savechain = c;
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

            if(position==273393){
                std::cout<<"__________"<<std::endl;
                std::cout<<"pos "<<position <<std::endl;
                std::cout<<"length "<<length <<std::endl;
                std::cout<<"n len "<< get_next_length() <<std::endl;
                std::cout<<"chain"<<std::bitset<16>(c.get_chain()) <<std::endl;
                 std::cout<<"__________"<<std::endl;
            }

           
            
            first = true;

        }
       uint16_t last_size;
        Chain savechain;
        uint16_t chain;
        len_t position;
        len_t src_position;
        uint16_t length;
        bool increasing;
        bool first;

        
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
            //std::cout<<chain<<std::endl;
            return 1 << (__builtin_ctz(chain));
        }
        inline len_t get_start_of_search()
        {
            if(position==584656){
                std::cout<<"__________"<<std::endl;
                std::cout<<"pos "<<position <<std::endl;
                std::cout<<"length "<<length <<std::endl;
                std::cout<<"n len "<< get_next_length() <<std::endl;
                 std::cout<<"__________"<<std::endl;
            }
            
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

        inline void absorp_next(len_t src)
        {
            if(chain==0){
                std::cout<<"__________"<<std::endl;
                std::cout<<"pos "<<position <<std::endl;
                std::cout<<"length "<<length <<std::endl;
                std::cout<<"n len "<< get_next_length() <<std::endl;
                 std::cout<<"__________"<<std::endl;
                 throw std::invalid_argument("chain=0");
                 std::cout<<"__________"<<std::endl;
            }
            len_t size = 1 << (__builtin_ctz(chain));
            chain = chain - size;

          
            if(increasing){
                src_position = src;
            }
            else{
                src_position = src+(size-length)+1;
            }

            length = length + size;

            // if (position < src_position)
            //         {
            //             std::cout<<"inc: " <<increasing<<" searchstat: "<<get_start_of_search()<<std::endl; 
            //             std::cout << "fpos: " << position << " fsrc: " << src_position <<" src: "<<src<< "f.len: " << length<<" oldl: "<<length-size << std::endl;
            //             throw std::invalid_argument("forawrdrefgroup:");
            //         }
            

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
            

            len_t old_position = position;
            len_t old_src_position = src_position;
            len_t old_length = length;
            len_t old_ss = get_start_of_search();
            uint16_t oldchain = chain;

            len_t size = 1 << (__builtin_ctz(chain));
            try{
            if (increasing)
            {
                position = position + length;
                chain = chain - size;
                length = size;
                src_position = position;

                if (old_position < old_src_position || position==209715008)
                    {
                        bool t =position==209715008;
                        std::cout<<"position==209715008: " <<t<<std::endl;
                        std::cout<<"inc: " <<increasing<<" searchstat: "<<get_start_of_search()<<std::endl; 
                        std::cout<<"oldss: "<<old_ss<<std::endl;
                        std::cout << "old_pos: " << old_position << " src: " << src_position <<" old_src: "<<old_src_position<< " len: " << length<< std::endl;
                        throw std::invalid_argument("forawrdrefgroup:");
                    }

                if (first)
                {
                    first = false;
                    //std::cout<<"1old_pos: "<<old_position<<std::endl;
                    //std::cout<<"1pos: "<<old_position <<" src: "<< old_src_position<<" l: "<<old_length<<std::endl;
                    return lz77Aprox::Factor(old_position, old_src_position, old_length, true);
                }
                else
                {
                    //std::cout<<"2pos: "<<old_position - old_length + 1<<" src: "<< old_src_position<<" l: "<<old_length<<std::endl;
                    return lz77Aprox::Factor(old_position, old_src_position, old_length, false);
                }
            }
            else
            {
                //decreasing
                position = position - length;
                chain = chain - size;
                length = size;
                src_position = position-length+1;

                if (old_position -old_length+1< old_src_position || position==209715008)
                    {
                         bool t =position==209715008;
                        std::cout<<"position==209715008: " <<t<<std::endl;
                        std::cout<<"inc: " <<increasing<<std::endl; 
                        std::cout<<"oldss: "<<old_ss<<" searchstat: "<<get_start_of_search()<<std::endl;
                        std::cout << "old_pos: " << old_position << " oldpos- " << old_position - old_length + 1<<std::endl;
                        std::cout << "pos: " << position <<std::endl;
                        std::cout <<" old_src_position: "<<old_src_position<< std::endl;
                        std::cout <<" src_position: "<<src_position<< std::endl;
                        std::cout <<" old_length: "<<old_length<< std::endl;
                        std::cout <<" length: "<<length<< std::endl;
                        std::cout <<" size: "<<size<< std::endl;

                        throw std::invalid_argument("forawrdrefgroup:");
                    }

                if (first)
                {
                    first = false;
                    //std::cout<<"3pos: "<<old_position<<" src: "<< old_src_position<<" l: "<<old_length<<std::endl;
                    return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length, true);
                }
                else
                {
                    //std::cout<<"4pos: "<<old_position<<" src: "<< old_src_position<<" l: "<<old_length<<std::endl;
                    return lz77Aprox::Factor(old_position - old_length + 1, old_src_position, old_length, false);
                }
            }
            // if(61440==chain){
            //     std::cout<<"size to large chain: "<<chain<<std::endl;
            //     throw std::invalid_argument( "chain to large group8" );
            // }

            }
            catch(std::invalid_argument &e){
                    
                        std::cout<<"__________"<<std::endl;
                std::cout<<"inc: " <<increasing<<std::endl; 
                        std::cout<<"oldss: "<<old_ss<<" searchstat: "<<get_start_of_search()<<std::endl;
                        std::cout << "old_pos: " << old_position << " oldpos- " << old_position - old_length + 1<<std::endl;
                        std::cout << "pos: " << position <<std::endl;
                        std::cout <<" old_src_position: "<<old_src_position<< std::endl;
                        std::cout <<" src_position: "<<src_position<< std::endl;
                        std::cout <<" old_length: "<<old_length<< std::endl;
                        std::cout <<" length: "<<length<< std::endl;
                        std::cout <<" size: "<<size<< std::endl;
                std::cout<<"n len "<< get_next_length() <<std::endl;
                std::cout<<"chain: "<<std::bitset<16>(get_chain()) <<std::endl;
                std::cout<<"oldchain: "<<std::bitset<16>(oldchain) <<std::endl;
                std::cout<<"c.pos: "<< savechain.get_position() <<std::endl;
                  std::cout<<"c.chain: "<<std::bitset<16>(savechain.get_chain()) <<std::endl;
                  std::cout<<"lastsize: "<<last_size<<"\n";
                 std::cout<<"__________"<<std::endl;
                 throw std::invalid_argument("g catch:");
                       
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