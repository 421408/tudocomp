#pragma once

#include <tudocomp/Algorithm.hpp>
#include <tudocomp/compressors/esp/SLPDepSort.hpp>
#include <tudocomp/compressors/esp/MonotoneSubsequences.hpp>

namespace tdc {namespace esp {
    class SufficientSLPStrategy: public Algorithm {
    public:
        inline static Meta meta() {
            Meta m("slp_strategy", "sufficient");
            return m;
        };

        using Algorithm::Algorithm;

        inline void encode(EspContext& context, SLP&& slp, Output& output) const {
            std::cout << "unsorted:\n";
            for (size_t i = 0; i < slp.rules.size(); i++) {
                std::cout
                    << i << ": "
                    << i + esp::GRAMMAR_PD_ELLIDED_PREFIX
                    << " -> (" << slp.rules[i][0] << ", " << slp.rules[i][1] << ")\n";
            }
            slp_dep_sort(slp); // can be implemented better, and in a way that yields
                               // temporary lists for reusal

            auto max_val = slp.rules.size() + esp::GRAMMAR_PD_ELLIDED_PREFIX - 1;
            auto bit_width = bits_for(max_val);

            BitOStream bout(output.as_stream());

            DCHECK_GE(bit_width, 1);
            DCHECK_LE(bit_width, 63); // 64-bit sizes are
                                    // restricted to 63 or less in practice

            if (slp.empty) {
                bit_width = 0;
                DCHECK(slp.rules.empty());
                DCHECK_EQ(slp.root_rule, 0);
            }

            // bit width
            bout.write_int(bit_width, 6);

            // size
            bout.write_int(max_val, bit_width);

            // root rule
            bout.write_int(slp.root_rule, bit_width);

            if (slp.empty || slp.root_rule < 256) {
                return;
            }

            std::cout << "sorted:\n";
            for (size_t i = 0; i < slp.rules.size(); i++) {
                std::cout
                    << i << ": "
                    << i + esp::GRAMMAR_PD_ELLIDED_PREFIX
                    << " -> (" << slp.rules[i][0] << ", " << slp.rules[i][1] << ")\n";
            }

            // ...
            //std::vector<size_t> rules_lhs;
            //std::vector<size_t> rules_lhs_diff;
            //rules_lhs.push_back(e[0]);
            //rules_lhs_diff.push_back(diff);
            //std::cout << "emitted lhs:   " << vec_to_debug_string(rules_lhs) << "\n";
            //std::cout << "emitted diffs: " << vec_to_debug_string(rules_lhs_diff) << "\n";

            // Write rules lhs
            {
                size_t last = 0;
                for (auto& e : slp.rules) {
                    DCHECK_LE(last, e[0]);
                    size_t diff = e[0] - last;
                    bout.write_unary(diff);
                    last = e[0];
                }
            }

            struct RhsAdapter {
                SLP* slp;
                inline const size_t& operator[](size_t i) const {
                    return slp->rules[i][1];
                }
                inline size_t size() const {
                    return slp->rules.size();
                }
            };
            const auto rhs = RhsAdapter { &slp };

            {
                std::vector<size_t> rules_rhs;
                for(size_t i = 0; i < rhs.size(); i++) {
                    rules_rhs.push_back(rhs[i]);
                }
                std::cout << "rhs: " << vec_to_debug_string(rules_rhs) << "\n";
            }

            // Sort rhs and create sorted indice array O(n log n)
            const auto sis = sorted_indices(rhs);

            std::cout << "sis: " << vec_to_debug_string(sis) << "\n";

            // Write rules rhs in sorted order (B array of encoding)
            {
                size_t last = 0;
                for (size_t i = 0; i < rhs.size(); i++) {
                    auto v = rhs[sis[i]];
                    DCHECK_LE(last, v);
                    size_t diff = v - last;
                    bout.write_unary(diff);
                    last = v;
                }
            }

            // Create Dpi and b
            std::vector<size_t> Dpi;
            auto b = IntVector<uint_t<1>> {};
            {
                auto tmp = esp::create_dpi_and_b_from_sorted_indices(sis);
                Dpi = std::move(tmp.Dpi);
                b = std::move(tmp.b);
            }
            size_t b_size = b.size();
            DCHECK_GE(b_size, 1);

            std::cout << "Dpi: " << vec_to_debug_string(Dpi) << "\n";
            std::cout << "b:   " << vec_to_debug_string(b) << "\n";

            // Write out b and discard it
            bout.write_compressed_int(b_size);
            for(uint8_t e : b) {
                bout.write_bit(e != 0);
            }
            b = IntVector<uint_t<1>> {};

            // transform Dpi to WT and write WT and discard WT
            {
                auto Dpi_bvs = make_wt(Dpi, b_size - 1);

                // write WT depth
                bout.write_compressed_int(Dpi_bvs.size());
                std::cout << "wt depth write: " << Dpi_bvs.size() << "\n";

                // write WT
                for(auto& bv : Dpi_bvs) {
                    for(uint8_t bit: bv) {
                        bout.write_bit(bit != 0);
                    }
                    std::cout << "Dpi bv: " << vec_to_debug_string(bv) << "\n";
                }
            }

            /*
            // Create Dsi from Dpi, discard Dpi,
            auto Dsi = esp::create_dsigma_from_dpi_and_sorted_indices(sis, Dpi);

            std::cout << "Dsi: " << vec_to_debug_string(Dsi) << "\n";

            Dpi = std::vector<size_t>();

            // transform Dsi to WT and write WT and discard WT
            {
                auto Dsi_bvs = make_wt(Dsi, b_size - 1);

                // write WT
                for(auto& bv : Dsi_bvs) {
                    for(uint8_t bit: bv) {
                        bout.write_bit(bit != 0);
                    }
                    //std::cout << "Dsi bv: " << vec_to_debug_string(bv) << "\n";
                }
            }

            // Discard Dpi
            Dsi = std::vector<size_t>();
            */

            std::cout << "encode OK\n";

        }

        inline SLP decode(Input& input) const {
            BitIStream bin(input.as_stream());

            // bit width
            auto bit_width = bin.read_int<size_t>(6);
            bool empty = (bit_width == 0);

            // size
            auto max_val = bin.read_int<size_t>(bit_width);

            // root rule
            auto root_rule = bin.read_int<size_t>(bit_width);

            //std::cout << "in:  Root rule: " << root_rule << "\n";
            //std::vector<size_t> rules_lhs_diff;
            //rules_lhs_diff.push_back(diff);
            //std::cout << "parsed lhs:   " << vec_to_debug_string(rules_lhs) << "\n";
            //std::cout << "parsed diffs: " << vec_to_debug_string(rules_lhs_diff) << "\n";

            esp::SLP slp;
            slp.empty = empty;
            slp.root_rule = root_rule;

            if (empty || slp.root_rule < 256) {
                return slp;
            }

            size_t slp_size = (max_val + 1) - 256; // implied initial bytes
            slp.rules.reserve(slp_size);
            slp.rules.resize(slp_size);

            // Read rules lhs
            {
                size_t last = 0;
                for(size_t i = 0; i < slp_size && !bin.eof(); i++) {
                    // ...
                    auto diff = bin.read_unary<size_t>();
                    last += diff;
                    slp.rules[i][0] = last;
                }
            }
            // Read rules rhs in sorted order (B array)
            std::vector<size_t> sis;
            sis.reserve(slp_size);
            {
                size_t last = 0;
                for(size_t i = 0; i < slp_size && !bin.eof(); i++) {
                    // ...
                    auto diff = bin.read_unary<size_t>();
                    last += diff;
                    sis.push_back(last);
                }
            }
            std::cout << vec_to_debug_string(sis) << "\n";

            // Read b
            size_t b_size = bin.read_compressed_int<size_t>();
            auto b = IntVector<uint_t<1>> {};
            b.reserve(b_size);
            b.resize(b_size);
            for(size_t i = 0; i < b_size; i++) {
                b[i] = bin.read_bit();
            }
            std::cout << "decomp b: " << vec_to_debug_string(b) << "\n";
            std::cout << "ok " << __LINE__ << "\n";

            // Read WT dept
            size_t wt_depth = bin.read_compressed_int<size_t>();

            std::cout << "ok " << __LINE__ << "\n";
            std::cout << wt_depth << "\n";

            // Read Dpi WT
            auto Dpi = std::vector<size_t>();
            {
                auto Dpi_bvs = std::vector<IntVector<uint_t<1>>>();
                Dpi_bvs.reserve(wt_depth);
                Dpi_bvs.resize(wt_depth);

                //std::cout << "ok " << __LINE__ << "\n";

                for(auto& bv : Dpi_bvs) {
                    bv.reserve(sis.size());

                    //std::cout << "ok " << __LINE__ << "\n";

                    for(size_t i = 0; i < sis.size(); i++) {
                        bv.push_back(bin.read_bit());
                    }
                    //std::cout << "Dpi bv: " << vec_to_debug_string(bv) << "\n";
                }

                Dpi = esp::recover_Dxx(Dpi_bvs, sis.size(), b_size - 1);
            }

            //std::cout << "ok " << __LINE__ << "\n";
            std::cout << "Dpi: " << vec_to_debug_string(Dpi) << "\n";

            /*
            // Read Dsi WT
            auto Dsi = std::vector<size_t>();
            {
                auto Dsi_bvs = std::vector<IntVector<uint_t<1>>>();
                Dsi_bvs.reserve(wt_depth);
                Dsi_bvs.resize(wt_depth);

                for(auto& bv : Dsi_bvs) {
                    bv.reserve(sis.size());
                    for(size_t i = 0; i < sis.size(); i++) {
                        bv.push_back(bin.read_bit());
                    }
                    //std::cout << "Dsi bv: " << vec_to_debug_string(bv) << "\n";
                }

                Dsi = esp::recover_Dxx(Dsi_bvs, sis.size(), b_size - 1);
            }
            std::cout << "Dsi: " << vec_to_debug_string(Dsi) << "\n";
            */

            std::cout << "ok " << __LINE__ << "\n";

            return esp::SLP();
        }
    };
}}

