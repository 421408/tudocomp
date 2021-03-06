#pragma once

#include <tudocomp/util.hpp>
#include <tudocomp/Compressor.hpp>
#include <tudocomp/Range.hpp>

#include <tudocomp_stat/StatPhase.hpp>

#include <tudocomp/compressors/lz_pointer_jumping/FixedBufferPointerJumping.hpp>
#include <tudocomp/compressors/lz_pointer_jumping/DynamicBufferPointerJumping.hpp>
#include <tudocomp/compressors/lz_pointer_jumping/PointerJumping.hpp>

// For default params
#include <tudocomp/compressors/lz_trie/TernaryTrie.hpp>
#include <tudocomp/coders/BinaryCoder.hpp>

namespace tdc {namespace lz_pointer_jumping {

template<typename lz_algo_t, typename coder_t, typename dict_t>
class BaseLZPointerJumpingCompressor: public Compressor {
    using factorid_t = lz_common::factorid_t;
    using node_t = typename dict_t::node_t;
    using encoder_t = typename coder_t::Encoder;
    struct stats_t {
        IF_STATS(size_t dictionary_resets = 0);
        IF_STATS(size_t dict_counter_at_last_reset = 0);
        IF_STATS(size_t total_factor_count = 0);
    };

    using lz_state_t = typename lz_algo_t::template lz_state_t<encoder_t, dict_t, stats_t>;
    using pointer_jumping_t = PointerJumping<DynamicBufferPointerJumping<lz_state_t>>;

    /// Max dictionary size before reset, 0 == unlimited
    const factorid_t m_dict_max_size {0};

    /// Pointer Jumping jump width.
    const size_t m_jump_width {1};

public:
    inline BaseLZPointerJumpingCompressor(Config&& cfg):
        Compressor(std::move(cfg)),
        m_dict_max_size(this->config().param("dict_size").as_uint()),
        m_jump_width(this->config().param("jump_width").as_uint())
    {
        CHECK_GE(m_jump_width, 1);
        CHECK_LE(m_jump_width, pointer_jumping_t::MAX_JUMP_WIDTH);
        CHECK_EQ(m_dict_max_size, 0) << "dictionary resets are currently not supported";
        // TODO: need to fix and include this code fragment for dictionary resets
        /*
        // check if dictionary's maximum size was reached
        // (this will never happen if m_dict_max_size == 0)
        if(tdc_unlikely(dict.size() == m_dict_max_size)) {
            DCHECK_GT(dict.size(),0U);
            reset_dict();
            factor_count = 0;
            IF_STATS(stat_dictionary_resets++);
            IF_STATS(stat_dict_counter_at_last_reset = m_dict_max_size);
        }
        */
    }

    inline static Meta meta() {
        Meta m(Compressor::type_desc(),
               lz_algo_t::meta_name(),
               lz_algo_t::meta_desc());
        m.param("coder", "The output encoder.")
            .strategy<coder_t>(TypeDesc("coder"), Meta::Default<BinaryCoder>());
        m.param("lz_trie", "The trie data structure implementation.")
            .strategy<dict_t>(TypeDesc("lz_trie"),
                Meta::Default<lz_trie::TernaryTrie>());
        m.param("dict_size",
            "the maximum size of the dictionary's backing storage before it "
            "gets reset (0 = unlimited)"
        ).primitive(0);
        m.param("jump_width",
            "jump width of the pointer jumping optimization"
        ).primitive(2);
        return m;
    }

    virtual void compress(Input& input, Output& out) override {
        const size_t n = input.size();
        const size_t reserved_size = isqrt(n)*2;
        auto is = input.as_stream();

        // Stats
        StatPhase phase(lz_algo_t::stat_phase_name());
        stats_t stats;

        size_t factor_count = 0;

        // set up coder
        encoder_t coder(config().sub_config("coder"), out, NoLiterals());

        // set up dictionary (the lz trie)
        dict_t dict(config().sub_config("lz_trie"), n, reserved_size + lz_state_t::initial_dict_size());

        // set up lz algorithm state
        lz_state_t lz_state { factor_count, coder, dict, stats };

        // set up initial state for trie search
        lz_state.reset_dict();
        bool early_exit = lz_state.initialize_traverse_state(is);
        if (early_exit) return;

        // set up pointer jumping
        pointer_jumping_t pjm { lz_state, m_jump_width };
        pjm.reset_buffer();

        // main loop
        char c;
        continue_while: while(is.get(c)) {
            auto action = pjm.on_insert_char(static_cast<uliteral_t>(c));
            if (action.buffer_full_and_found()) {
                // we can jump ahead
                lz_state.set_traverse_state(action.traverse_state());
                pjm.reset_buffer();
            } else if (action.buffer_full_and_not_found()) {
                // we need to manually add to the trie,
                // and create a new jump entry
                for(size_t i = 0; i < pjm.jump_buffer_size() - 1; i++) {
                    uliteral_t const bc = pjm.jump_buffer(i);
                    bool is_new_node = lz_state.dict_find_or_insert(bc);
                    if (is_new_node) {
                        // we got a new trie node in the middle of the
                        // jump buffer, restart the jump buffer search
                        lz_state.reset_traverse_state(bc);
                        pjm.shift_buffer(i + 1);
                        goto continue_while;
                    }
                }
                {
                    // the child node for the last char in the buffer
                    // is also the node the new jump pointer jumps to.
                    size_t i = pjm.jump_buffer_size() - 1;
                    uliteral_t const bc = pjm.jump_buffer(i);
                    bool is_new_node = lz_state.dict_find_or_insert(bc);

                    // the next time we will skip over this through the jump pointer
                    DCHECK(is_new_node);

                    pjm.insert_jump_buffer(lz_state.get_traverse_state());

                    lz_state.reset_traverse_state(bc);
                    pjm.reset_buffer();
                }
            } else {
                // read next char...
            }
        }

        // process chars from last incomplete jump buffer
        for(size_t i = 0; i < pjm.jump_buffer_size(); i++) {
            uliteral_t const bc = pjm.jump_buffer(i);
            bool is_new_node = lz_state.dict_find_or_insert(bc);
            if (is_new_node) {
                lz_state.reset_traverse_state(bc);
            }
        }

        // take care of left-overs. We do not assume that the stream has a sentinel
        lz_state.end_of_input(static_cast<uliteral_t>(c));

        IF_STATS(
            phase.log_stat("factor_count",
                           stats.total_factor_count);
            phase.log_stat("dictionary_reset_counter",
                           stats.dictionary_resets);
            phase.log_stat("max_factor_counter",
                           stats.dict_counter_at_last_reset);
        )
    }

    inline std::unique_ptr<Decompressor> decompressor() const override {
        using Decompressor = typename lz_algo_t::template Decompressor<coder_t>;
        // FIXME: construct AST and pass it
        std::stringstream cfg;
        cfg << "dict_size=" << to_string(m_dict_max_size);
        return Algorithm::instance<Decompressor>(cfg.str());
    }
};

}}//ns
