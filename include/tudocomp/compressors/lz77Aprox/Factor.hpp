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
    }

    bool operator < (const Factor& fac) const
    {
        return (pos < fac.pos);
    }
}  __attribute__((__packed__));

}}
