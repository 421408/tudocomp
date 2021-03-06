#pragma once

#include <vector>
#include <tudocomp/def.hpp>
#include <tudocomp/ds/IntVector.hpp>
#include <tudocomp/Algorithm.hpp>

#include <tudocomp/compressors/lcpcomp/lcpcomp.hpp>

namespace tdc {
namespace lcpcomp {

/**
 * Decodes lcpcomp compressed data as described in the paper.
 * It creates an array of dynamic arrays. Each dynamic array stores in
 * the first element its size. On appending a new element, the dynamic array
 * gets resized by one (instead of doubling). This helps to keep the memory footprint low.
 * It can be faster than \ref ScanDec if its "scans"-value too small.
 */
class CompactDec : public Algorithm {
public:
    inline static Meta meta() {
        Meta m(dec_strategy_type(), "compact");
        return m;
    }

private:
    len_compact_t** m_fwd;

    len_t m_cursor;
    IF_STATS(len_t m_longest_chain);
    IF_STATS(len_t m_current_chain);

    IntVector<uliteral_t> m_buffer;

    inline void decode_literal_at(len_t pos, uliteral_t c) {
		IF_STATS(++m_current_chain);
        IF_STATS(m_longest_chain = std::max(m_longest_chain, m_current_chain));

        m_buffer[pos] = c;
		DCHECK(c != 0 || pos == m_buffer.size()-1); // we assume that the text to restore does not contain a NULL-byte but at its very end

        if(m_fwd[pos] != nullptr) {
            const len_compact_t*const& bucket = m_fwd[pos];
            for(size_t i = 1; i < bucket[0]; ++i) {
                decode_literal_at(bucket[i], c); // recursion
            }
            free(m_fwd[pos]);
            m_fwd[pos] = nullptr;
        }

        IF_STATS(--m_current_chain);
    }

public:
    ~CompactDec() {
        if(m_fwd != nullptr) {
            for(size_t i = 0; i < m_buffer.size(); ++i) {
                if(m_fwd[i] == nullptr) continue;
                free(m_fwd[i]);
            }
            delete [] m_fwd;
        }
    }
    inline CompactDec(Config&& cfg)
        : Algorithm(std::move(cfg)), m_cursor(0) {

        IF_STATS(m_longest_chain = 0);
        IF_STATS(m_current_chain = 0);
    }

    inline void initialize(size_t n) {
        if(tdc_unlikely(n == 0)) throw std::runtime_error(
            "no text length provided");

        m_buffer.resize(n, 0);
        m_fwd = new len_compact_t*[n];
        std::fill(m_fwd,m_fwd+n,nullptr);
    }

    inline void decode_literal(uliteral_t c) {
        decode_literal_at(m_cursor++, c);
    }

    inline void decode_factor(len_t pos, len_t num) {
        for(len_t i = 0; i < num; i++) {
            len_t src = pos+i;
            if(m_buffer[src]) {
                decode_literal_at(m_cursor, m_buffer[src]);
            } else {
                len_compact_t*& bucket = m_fwd[src];
                if(bucket == nullptr) {
                    bucket = (len_compact_t*) malloc(sizeof(len_compact_t) * 2);
                    DCHECK(m_fwd[src] == bucket);
                    bucket[0] = 2;
					bucket[1] = m_cursor;
                }
				else
                { // this block implements the call of m_fwd[src]->push_back(m_cursor);
					++bucket[0]; // increase the size of a bucket only by one!
					bucket = (len_compact_t*) realloc(bucket, sizeof(len_compact_t)*bucket[0]);
                    bucket[bucket[0]-1] = m_cursor;
                }
            }

            ++m_cursor;
        }
    }

    inline void process() {
    }

    IF_STATS(
    inline len_t longest_chain() const {
        return m_longest_chain;
    })

    inline void write_to(std::ostream& out) const {
        for(auto c : m_buffer) out << c;
    }
};

}} //ns

