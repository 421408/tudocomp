#pragma once

#include <tudocomp/def.hpp>

namespace tdc {
    namespace lz77Aprox {

        class Factor {
        public:
            len_compact_t pos, src, len;


            inline Factor() {
                // undefined!
            }

            inline Factor(len_t fpos, len_t fsrc, len_t flen)
                    : pos(fpos), src(fsrc), len(flen) {

            }

            bool operator<(const Factor &fac) const {
                return (pos < fac.pos);
            }
            bool operator>(const Factor &fac) const {
                return (pos > fac.pos);
            }
        }  __attribute__((__packed__));

    }
}
