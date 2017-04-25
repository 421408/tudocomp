#pragma once

#include <tudocomp/compressors/esp/EspContext.hpp>
#include <tudocomp/compressors/esp/RoundContext.hpp>
#include <tudocomp/compressors/esp/meta_blocks.hpp>

#include <tudocomp/compressors/esp/utils.hpp>

namespace tdc {namespace esp {
    Rounds EspContext::generate_grammar_rounds(string_ref input) {
        std::vector<Round> rounds;
        size_t root_node = 0;
        bool empty = false;

        SLP slp;
        size_t slp_counter = 256;
        size_t prev_slp_counter = 0;

        // Initialize initial round
        {
            auto phase = with_env([&](auto& env) {
                return env.stat_phase("Prepare round 0");
            });

            Round round0 {
                GrammarRules(256),
                256, // TODO: Calc actual alphabet size
                std::vector<size_t>(),
            };
            round0.string.reserve(input.size());
            for (auto c : input) {
                round0.string.push_back(c);
            }
            rounds.push_back(std::move(round0));
        }

        for(size_t n = 0;; n++) {
            auto phase = with_env([&](auto& env) {
                std::stringstream ss;
                ss << "Round " << n;
                return env.stat_phase(ss.str());
            });

            Round& r = rounds.back();
            in_t in = r.string;

            esp::RoundContext<in_t> ctx {
                r.alphabet,
                in,
                behavior_metablocks_maximimze_repeating,
                behavior_landmarks_tie_to_right,
                debug.round(),
            };

            ctx.debug.init(n, in, r.alphabet);

            if (in.size() == 0) {
                empty = true;
                ctx.debug.last_round(0, true);
                break;
            }
            if (in.size() == 1) {
                root_node = in[0] + prev_slp_counter;
                ctx.debug.last_round(root_node, false);
                break;
            }

            std::vector<size_t> new_layer;

            ctx.split(in);

            const auto& v = ctx.adjusted_blocks();

            ctx.debug.slice_symbol_map_start();
            {
                in_t s = in;
                for (auto e : v) {
                    auto slice = s.slice(0, e.len);
                    s = s.slice(e.len);
                    auto rule_name = r.gr.add(slice) - (r.gr.m_initial_counter - 1);

                    ctx.debug.slice_symbol_map(slice, rule_name);

                    new_layer.push_back(rule_name);
                }
            }

            // Delete previous string
            r.string = std::vector<size_t>();

            // TODO: Preallocate based on number of rules
            new_layer.shrink_to_fit();

            // Append to slp array
            {
                size_t old_slp_size = slp.rules.size();
                size_t additional_slp_size = r.gr.rules_count();
                size_t new_slp_size = old_slp_size + additional_slp_size;

                slp.rules.reserve(new_slp_size);
                slp.rules.resize(new_slp_size);

                auto& rv = slp.rules;
                for(auto& kv : r.gr.n2) {
                    const auto& key = kv.first;
                    const auto& val = kv.second - r.gr.m_initial_counter;

                    size_t store_idx = slp_counter + val - 256;
                    rv[store_idx][0] = key.m_data[0] + prev_slp_counter;
                    rv[store_idx][1] = key.m_data[1] + prev_slp_counter;
                }

                prev_slp_counter = slp_counter;
                slp_counter += additional_slp_size;
            }

            // Prepare next round
            rounds.push_back(Round {
                GrammarRules(r.gr.rules_count()),
                r.gr.rules_count(),
                std::move(new_layer),
            });
        }

        slp.empty = empty;
        slp.root_rule = root_node;

        return Rounds {
            std::move(rounds),
            root_node,
            empty,
            slp,
        };
    }

    ///
    /// REFACTOR: ONE GLOBAL HASHMAP STRUCTURE WITH GLOBAL ARRAY INDICE CONTENTS
    /// SUBTRACT DOWN TO LAYER_INDEX MANUALLY WHERE NEEDED
    ///
    /// NEW GOAL
    ///
    /// direct hashmap -> slp conversion, with push-to-end and
    /// remembering original start symbol

    // TODO: Either ellided-256 alphabet, or compressed included alphabet

    SLP EspContext::generate_grammar(const Rounds& rds) {
        return std::move(rds.slp);
    }
}}
