#pragma once

#include <string>
#include <vector>
#include <tudocomp/util/type_list.hpp>
#include <tudocomp/meta/TypeDesc.hpp>

namespace tdc {

using dsid_t = size_t;
using dsid_list_t = std::vector<dsid_t>;

namespace ds {
    constexpr dsid_t SUFFIX_ARRAY  = 0;
    constexpr dsid_t INVERSE_SUFFIX_ARRAY = 1;
    constexpr dsid_t LCP_ARRAY = 2;
    constexpr dsid_t PHI_ARRAY = 3;
    constexpr dsid_t PLCP_ARRAY = 4;

    constexpr TypeDesc type() {
        return TypeDesc("ds");
    }

    constexpr TypeDesc provider_type() {
        return TypeDesc("ds_provider");
    }

    inline std::string name_for(dsid_t id) {
        switch(id) {
            case SUFFIX_ARRAY:         return "suffix_array";
            case INVERSE_SUFFIX_ARRAY: return "inverse_suffix_array";
            case LCP_ARRAY:            return "lcp_array";
            case PHI_ARRAY:            return "phi_array";
            case PLCP_ARRAY:           return "plcp_array";
            default:
                return std::string("#") + std::to_string(id);
        }
    }
}

}

