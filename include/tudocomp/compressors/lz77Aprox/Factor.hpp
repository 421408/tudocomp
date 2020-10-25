#pragma once

#include <tudocomp/def.hpp>

namespace tdc {
namespace lz77Aprox {

class Factor {
public:
    len_compact_t pos, src, len;
    bool border;

    inline Factor() {
        // undefined!
    }

    inline Factor(len_t fpos, len_t fsrc, len_t flen,bool brd)
        : pos(fpos), src(fsrc), len(flen) ,border(brd) {
             if (fpos < fsrc)
                    {
                        
                        std::cout << "fpos: " << fpos<< " fsrc: " << fsrc <<" flen: " << flen<< std::endl;
                        
                    }
                    if(fpos==273393){
                        std::cout << "facgen false fpos: " << fpos<< " fsrc: " << fsrc <<" flen: " << flen<< std::endl;
                       
                throw std::invalid_argument("facgen:");
            }
    }

    bool operator < (const Factor& fac) const
    {
        return (pos < fac.pos);
    }
}  __attribute__((__packed__));

}}
