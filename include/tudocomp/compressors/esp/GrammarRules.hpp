#pragma once

#include <tudocomp/compressors/esp/HashArray.hpp>

namespace tdc {namespace esp {
    struct GrammarRules {
        GrammarRules() {}
        using a2_t = std::unordered_map<Array<2>, size_t>;
        using a3_t = std::unordered_map<Array<3>, size_t>;

        a2_t n2;
        a3_t n3;

        size_t counter = 1;

        size_t add(in_t v) {
            size_t* r;
            if (v.size() == 2) {
                r = &n2[v];
            } else if (v.size() == 3) {
                r = &n3[v];
            } else {
                DCHECK(false);
            }

            if (*r == 0) {
                *r = counter++;
            }

            return *r - 1;
        }
    };
}}
