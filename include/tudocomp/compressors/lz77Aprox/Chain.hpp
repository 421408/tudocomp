#pragma once

#include <tudocomp/def.hpp>


namespace tdc{

 class Chain {
    public:

        Chain(len_compact_t length,len_compact_t pos) {
            chain =0;
            positon =pos;
             if(pos==273393){
                std::cout<<"old pos: "<<positon<<" newpos: "<<pos <<"\n";
            }
        }
        Chain() {
            chain =0;
        }

        uint16_t chain;
        len_compact_t positon;

        inline len_compact_t get_position(){
            return positon;
        }
        inline void set_position(len_compact_t pos){
             if(pos==273393){
                std::cout<<"old pos: "<<positon<<" newpos: "<<pos <<"\n";
            }
            
            positon=pos;
           
        }

        inline void add(len_compact_t &adder){
            chain=chain+adder;
            // if(adder>4096){
            //     std::cout<<"adder: "<<adder<<std::endl;
            //     throw std::invalid_argument( "adder to large" );
            // }
            // if(chain>=8176){
            //     std::cout<<"adder: "<<adder<<std::endl;
            //     throw std::invalid_argument( "dickhead" );
            // }
            
        }

        inline uint16_t get_chain(){
            return chain;
        }
    }__attribute__((__packed__));
}//ns