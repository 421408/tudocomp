#pragma once

#include <tudocomp/util.hpp>
#include <tudocomp/Env.hpp>
#include <tudocomp/Compressor.hpp>
#include <tudocomp/ds/IntVector.hpp>

namespace tdc {
namespace esp {
    // wether to maximize or minimize repeating meta blocks
    const bool MAXIMIZE_REPEATING = true;

    // Wether to tie landmark-blocks to the right or to the left
    const bool TIE_TO_RIGHT = true;

    // Wether to detect block 2 suffixes of size 1 and merge them to the type 3 prefix
    const bool ADJUST_SIZE_1_BLOCK_2_SUFFIX = true;

    /*

    Paramters:
    - treatment of with-1 metablocks
    - repeating metablock large or small
        [aaa|sdfgh|aaa]
        vs [aa|asdfgha|aa}
    - Tests for cases
        [aa|bababa] vs [a|abababa], eg "aabababababababaa"

    */

    using int_vector::IntVector;

    // Implementation that covers all of 64 bit
    // TODO: Does the Paper mean base-e or base-2 ?
    inline size_t iter_log(size_t n) {
        if (n < 3) return 1;
        if (n < 16) return 2;
        if (n < 3814280) return 3;
        return 4;
    }

    template<class T>
    struct LabeledBlock {
        size_t type;
        ConstGenericView<T> view;
    };

    void print_labeled_blocks(const std::vector<LabeledBlock<uint8_t>>& labeled_blocks) {
        std::cout << "|";
        for (auto& mb : labeled_blocks) {
            size_t w = mb.view.size();

            if (w <= 2) {
                std::cout << std::right << std::setw(w) << mb.type;
            } else {
                w /= 2;
                std::cout << std::setw(w) << "";
                std::cout << std::left << std::setw(mb.view.size() - w) << mb.type;
            }

            std::cout << "|";
        }
        std::cout << "\n";

        std::cout << "|";
        for (auto& mb : labeled_blocks) {
            std::cout << mb.view << "|";
        }
        std::cout << "\n";
    }

    template<class T>
    void print_labeled_blocks2(const std::vector<LabeledBlock<T>>& labeled_blocks) {
        std::vector<std::string> fragments;
        for (auto& mb : labeled_blocks) {
            std::stringstream ss;
            bool first = true;
            ss << " ";
            for (auto e : mb.view) {
                if (!first) {
                    ss << ", ";
                }
                ss << e;
                first = false;
            }
            ss << " ";
            fragments.push_back(ss.str());
        }

        std::cout << "|";
        for (size_t i = 0; i < labeled_blocks.size(); i++) {
            size_t w = fragments[i].size();
            auto& mb = labeled_blocks[i];

            if (w <= 2) {
                std::cout << std::right << std::setw(w) << mb.type;
            } else {
                w /= 2;
                std::cout << std::setw(w) << "";
                std::cout << std::left << std::setw(fragments[i].size() - w) << mb.type;
            }

            std::cout << "|";
        }
        std::cout << "\n";

        std::cout << "|";
        for (size_t i = 0; i < labeled_blocks.size(); i++) {
            std::cout << fragments[i] << "|";
        }
        std::cout << "\n";
    }

    template<class T>
    bool initial_labeled_blocks_debug(const std::vector<LabeledBlock<T>> meta_blocks,
                                    ConstGenericView<T> in) {
        std::cout << "\nLabeled blocks:\n";

        bool ok = false;
        {
            std::vector<T> ss;
            for (auto& mb : meta_blocks) {
                for (auto& e : mb.view) {
                    ss.push_back(e);
                }
            }
            ok = (ConstGenericView<T>(ss) == in);
        }

        print_labeled_blocks2(meta_blocks);

        return ok;
    }

    template<class T>
    uint64_t calc_alphabet_size(const T& t) {
        Counter<typename T::value_type> c;
        for (auto v : t) {
            c.increase(v);
        }
        return c.getNumItems();
    }

    template<class T>
    bool no_adjacent_identical(const T& t) {
        for(size_t i = 1; i < t.size(); i++) {
            if (t[i] == t[i - 1]) return false;
        }
        return true;
    }

    uint64_t label(uint64_t left, uint64_t right) {
        auto diff = left ^ right;

        //std::cout << "l: " << std::setbase(2) << left << "\n";
        //std::cout << "r: " << std::setbase(2) << right << "\n";
        //std::cout << "d: " << std::setbase(2) << diff << "\n";
        //std::cout << "\n";


        DCHECK(diff != 0);

        auto l = __builtin_ctz(diff);

        auto bit = [](uint8_t l, uint64_t v) {
            // TODO: test
            return (v >> l) & 1;
        };

        // form label(A[i])
        return 2*l + bit(l, right);
    };

    template<class T, class F>
    void for_neigbors(T& t, F f) {
        for (size_t i = 0; i < t.size(); i++) {
            typename T::value_type neighbors[2];
            uint8_t neighbor_len = 0;

            if (i == 0 && i == t.size() - 1) {
                neighbor_len = 0;
            } else if (i == 0) {
                neighbor_len = 1;
                neighbors[0] = t[i + 1];
            } else if (i == t.size() - 1) {
                neighbor_len = 1;
                neighbors[0] = t[i - 1];
            } else {
                neighbor_len = 2;
                neighbors[0] = t[i - 1];
                neighbors[1] = t[i + 1];
            }

            f(i, neighbors, neighbor_len);
        }
    }

    template<class T>
    bool check_landmarks(const T& t, bool allow_long = false) {
        if (allow_long) return true;
        size_t last = 0;
        size_t i = 0;
        for(; i < t.size(); i++) {
            if (t[i] == 1u) {
                if (i > 1) return false;
                last = i;
                i++;
                break;
            }
        }
        for(; i < t.size(); i++) {
            if (t[i] == 1u) {
                if ((i - last) > 3 || (i - last) < 2) return false;
                last = i;
            }
        }
        return true;
    }

    // NB: Thuis assumes iter_log(alphabet_size) valid bytes before A
    template<class T, class G>
    inline void handle_meta_block_2(ConstGenericView<T> A,
                                    uint64_t alphabet_size,
                                    std::vector<T>& buf,
                                    G push_block) {
        DCHECK(A.size() > 0);
        auto type_3_prefix = iter_log(alphabet_size);
        A = ConstGenericView<T>(A.data() - type_3_prefix, A.size() + type_3_prefix);
        buf.clear();
        buf.reserve(A.cend() - A.cbegin());
        buf.insert(buf.cbegin(), A.cbegin(), A.cend());

        std::cout << "  " << vec_to_debug_string(buf) << "\n";
        std::cout << "  " << "Reduce to 6:\n";

        for (uint shrink_i = 0; shrink_i < iter_log(alphabet_size); shrink_i++) {
            for (size_t i = 1; i < buf.size(); i++) {
                auto left  = buf[i - 1];
                auto right = buf[i];
                buf[i - 1] = label(left, right);
            }
            buf.pop_back();

            std::cout << "  " << vec_to_debug_string(buf) << "\n";
        }

        DCHECK(calc_alphabet_size(buf) <= 6);

        std::cout << "  " << "Reduce to 3:\n";

        // TODO: This would benefit from a general, mutable, slice type

        // final pass: reduce to alphabet 3
        for(uint to_replace = 3; to_replace < 6; to_replace++) {
            for_neigbors(buf, [&](size_t i, T neighbors[], uint8_t neighbor_len) {
                auto& e = buf[i];
                if (e == to_replace) {
                    e = 0;
                    for (uint8_t j = 0; j < neighbor_len; j++) {
                        if (neighbors[j] == e) {
                            e++;
                        }
                    }
                    for (uint8_t j = 0; j < neighbor_len; j++) {
                        if (neighbors[j] == e) {
                            e++;
                        }
                    }
                }
            });

            std::cout << "  " << vec_to_debug_string(buf) << "\n";
        }

        DCHECK(calc_alphabet_size(buf) <= 3);
        DCHECK(no_adjacent_identical(buf));

        // find landmarks:

        // TODO: Maybe store in high bits of buf to reduce memory?
        // buf gets reduced to 2 bit values anyway, and stays around long enough
        IntVector<uint_t<1>> landmarks(buf.size());

        for_neigbors(buf, [&](size_t i, T neighbors[], uint8_t neighbor_len) {
            bool is_high_landmark = true;
            for (uint8_t j = 0; j < neighbor_len; j++) {
                if (neighbors[j] > buf[i]) {
                    is_high_landmark = false;
                }
            }
            if (is_high_landmark) {
                landmarks[i] = 1;
            }
        });

        std::cout << "  High Landmarks:\n";
        std::cout << "  " << vec_to_debug_string(landmarks) << "\n";

        for_neigbors(buf, [&](size_t i, T neighbors[], uint8_t neighbor_len) {
            bool is_low_landmark = true;
            for (uint8_t j = 0; j < neighbor_len; j++) {
                if (neighbors[j] < buf[i]) {
                    is_low_landmark = false;
                }
            }
            if (is_low_landmark) {
                if (   (!(i > 0)              || (landmarks[i - 1] == 0u))
                    && (!(i < buf.size() - 1) || (landmarks[i + 1] == 0u))
                ) {
                    landmarks[i] = 1;
                }
            }
        });

        std::cout << "  High and Low Landmarks:\n";
        std::cout << "  " << vec_to_debug_string(landmarks) << "\n";

        DCHECK(check_landmarks(landmarks));

        // assign blocks

        /*
        size_t last = 0;
        // we can cut iteration time in half by making use of the 2-3
        // difference between each landmark
        for(size_t i = 0; i < landmarks.size(); i += 2) {
            if (landmarks[i] == 0u) {
                i++;
            }
            DCHECK(i < landmarks.size());
            DCHECK(landmarks[i] == 1u);

            last = i;
        }

        */

        // TODO: An abstraction for iterating while
        // having special setup/teardown sections
        // for the first N and last M elements, as well if the array is only O long

        // TODO: debug only
        std::vector<size_t> debug_landmark_assoc(buf.size());

        size_t last_closes_landmark = 0;
        for (size_t i = 1; i < buf.size(); i++) {
            if (landmarks[i] == 1u) {
                last_closes_landmark = i;
            }
            debug_landmark_assoc[i - 1] = last_closes_landmark;
        }
        if (debug_landmark_assoc.size() > 0) {
            debug_landmark_assoc.back() = last_closes_landmark;
        }

        std::cout << "  Block-Landmark Assignment:\n";
        std::cout << "  " << vec_to_debug_string(debug_landmark_assoc) << "\n";

        if (debug_landmark_assoc.size() > 0) {
            auto block_range = [&](size_t a, size_t b) {
                //std::cout << a << " - " << b << "\n";
                push_block(A.slice(type_3_prefix).slice(a, b));
            };

            size_t last_pos = 0;
            for(size_t i = 0; i < debug_landmark_assoc.size() - 1; i++) {
                if (debug_landmark_assoc[i] != debug_landmark_assoc[i + 1]) {
                    block_range(last_pos, i + 1);
                    last_pos = i + 1;
                }
            }
            block_range(last_pos, debug_landmark_assoc.size());
        }

    }

    template<class T, class G>
    inline void handle_meta_block_13(ConstGenericView<T> A,
                                    G push_block) {
        while (true) {
            auto s = A.size();

            // TODO: Decide how to handle
            DCHECK(s > 1);
            if (s == 1) { push_block(A); return; }

            if (s == 2) {
                push_block(A);
                return;
            } else if (s == 3) {
                push_block(A);
                return;
            } else if (s == 4) {
                push_block(A.slice(0, 2));
                push_block(A.slice(2));
                return;
            } else {
                push_block(A.slice(0, 3));
                A = A.slice(3);
            }
        }
    }

    template<class T>
    inline std::vector<ConstGenericView<T>> partition(ConstGenericView<T> in, size_t alphabet_size) {
        using GView = ConstGenericView<T>;

        std::vector<GView> blocks;
        std::vector<LabeledBlock<T>> meta_blocks;

        auto push_block = [&](GView A) {
            blocks.push_back(A);
        };

        auto type_23_prefix = iter_log(alphabet_size);

        // Merge length-one-block with adjacent one
        auto middle_meta_block = [&](uint8_t type, GView A) {
            // Attach to right meta block if not attached to left one
            if (meta_blocks.size() > 0 && meta_blocks.back().view.size() == 1) {
                meta_blocks.pop_back();
                A = GView(A.data() - 1, A.size() + 1);
            }

            // Attach to left meta block before attempting to attach to right one
            if (type != 2 && A.size() == 1) {
                if (meta_blocks.size() > 0 && meta_blocks.back().type == 1) {
                    auto& v = meta_blocks.back().view;
                    v = GView(v.data(), v.size() + 1);
                } else {
                    meta_blocks.push_back(LabeledBlock<T> { type, A });
                }
            } else {
                meta_blocks.push_back(LabeledBlock<T> { type, A });
            }
        };

        // Split long into short and long
        auto initial_meta_block = [&](uint8_t type, GView A) {
            if (type == 2) {
                middle_meta_block(3, A.slice(0, type_23_prefix));
                A = A.slice(type_23_prefix);
                if (A.size() > 0) {
                    middle_meta_block(2, A);
                }
            } else {
                middle_meta_block(type, A);
            }
        };

        // Split into repeating, long and short
        size_t i = 0;
        while(i < in.size()) {
            auto type_1_start = i;
            while ((i < (in.size() - 1)) && (in[i] == in[i + 1])) {
                i++;
            }
            if ((i - type_1_start) > 0) {
                GView A = in.slice(type_1_start, i + 1);
                {
                    initial_meta_block(1, A);
                }
                i++;
            }

            auto type_23_start = i;
            while (i < (in.size() - 1) && in[i] != in[i + 1]) {
                i++;
            }

            if (i == in.size() - 1) {
                i++;
            }
            size_t type_23_len = i - type_23_start;
            if (type_23_len > 0) {
                GView A = in.slice(type_23_start, i);

                if (type_23_len >= iter_log(alphabet_size)) {
                    initial_meta_block(2, A);
                } else {
                    initial_meta_block(3, A);
                }
            }
        }

        std::vector<T> buf;

        // Final processing, without any more meta block splitting
        auto real_meta_block = [&](uint8_t type, GView A) {
            if (type == 2) {
                handle_meta_block_2(A, alphabet_size, buf, push_block);
            } else {
                // TODO: Decide how to handle
                DCHECK(A.size() > 1 /* 1-byte input not covered by paper */);
                handle_meta_block_13(A, push_block);
            }

            std::cout << "  ---\n";
        };

        //DCHECK(initial_labeled_blocks_debug(meta_blocks, in));

        for (auto mb : meta_blocks) {
            real_meta_block(mb.type, mb.view);
        }

        return blocks;
    }

    template<class T>
    struct BlockLabelVec {
        std::vector<LabeledBlock<T>> vec;
        uint64_t alphabet_size;
    };


    template<class T>
    struct BlockLabelMap {
        std::unordered_map<ConstGenericView<T>, uint64_t> map;
        uint64_t alphabet_size;
    };

    template<class T>
    inline BlockLabelMap<T> block_label(const std::vector<ConstGenericView<T>>& blocks) {
        uint64_t counter = 0;
        std::unordered_map<ConstGenericView<T>, uint64_t> map;

        for (const auto& block: blocks) {
            if (map.count(block) == 0) {
                map[block] = counter;
                counter++;
            }
        }

        return BlockLabelMap<T> { map, counter };
    }

    template<class T>
    BlockLabelVec<T> labeled_partition(ConstGenericView<T> in, size_t alphabet_size){
        auto blocks = partition(in, alphabet_size);
        auto labels = block_label(blocks);

        std::vector<LabeledBlock<T>> final_labels;
        for (auto block : blocks) {
            final_labels.push_back(LabeledBlock<T> { labels.map[block], block });
        }

        return BlockLabelVec<T> { final_labels, labels.alphabet_size };
    }

    struct TypedBlock {
        uint8_t len;
        uint8_t type;
    };

    void adjust_blocks(std::vector<TypedBlock>& blocks) {

    }

    template<typename Source>
    struct Context {
        size_t alphabet_size;
        std::vector<size_t> scratchpad;
        Source s;
        Source sb;
        size_t i = 0;
        size_t last_i = 0;
        bool print_mb_trace = true;
        bool print_mb2_trace = true;

        std::vector<TypedBlock> block_buffer;

        Context(size_t as, Source src):
            alphabet_size(as),
            scratchpad(),
            s(src),
            sb(src),
            i(0),
            last_i(0)
        {}

        void reset_debug_print() {
            sb = s;
        }

        void print_cut(size_t l, size_t type) {
            auto front_cut = sb.substr(0, l);
            auto back_cut = sb.substr(l);

            auto n = s.size() - (back_cut.size() + front_cut.size());

            IF_DEBUG(if (print_mb_trace) {
                std::cout << "mblock " << type << ": ";
                std::cout << std::setw(n) << "";
                std::cout << "[" << front_cut;
                std::cout << "] [" << back_cut;
                std::cout << "]\n";
            })

            sb = back_cut;

            if (l < 2 || l > 3) std::cout << "Needs adjustment!\n";
        }

        void print_all() {
            reset_debug_print();
            for (auto& b : block_buffer) {
                print_cut(b.len, b.type);
            }
        }

        void push_back(size_t l, size_t type) {
            print_cut(l, type);
            i += l;
            block_buffer.push_back(TypedBlock { l, type });
        }

        void check_advanced(size_t len) {
            DCHECK_EQ(i - last_i, len);
            last_i = i;
        }

        std::vector<TypedBlock>& adjusted_blocks() {
            adjust_blocks(block_buffer);

            print_all();
            for (auto& b: block_buffer) {
                DCHECK_GE(b.len, 2);
                DCHECK_LE(b.len, 3);
            }
            return block_buffer;
        }
    };

    template<typename LmPred, typename SpanPush>
    void landmark_spanner(size_t size,
                          LmPred pred,
                          SpanPush push,
                          bool tie) {
        struct Block {
            size_t left;
            size_t right;
            inline size_t size() { return right - left + 1; }
        };
        std::array<Block, 3> blocks {{
            {0, 0},
            {0, 0},
            {0, 0},
        }};
        uint8_t bi = 0;

        for(size_t i = 0; i < size; i++) {
            if (pred(i)) {
                blocks[2].left  = (i == 0)        ? (i) : (i - 1);
                blocks[2].right = (i == size - 1) ? (i) : (i + 1);
                /*std::cout <<
                    "i: " << i << ", "
                    "old: (" << left << ", " << right << "), "
                    "new: (" << new_left << ", " << new_right << ")\n";*/

                if (bi > 0) {
                    // Adjust overlap of adjacent landmarks
                    if (blocks[2].left == blocks[1].right) {
                        if (tie) {
                            blocks[1].right--;
                        } else {
                            blocks[2].left++;
                        }
                    }

                    // Adjust edge cases to prevent size-1 blocks
                    // [1] [333] -> [22] [22]
                    // [1] [22] -> [333]
                    if (bi > 1) {
                        if (blocks[0].size() == 1) {
                            if (blocks[1].size() == 3) {
                                blocks[0].right++;
                                blocks[1].left++;
                            } else {
                                blocks[1].left--;
                                bi = 1;
                            }
                        }
                    }
                }

                if (bi == 0) {
                    bi = 1;
                } else if (bi == 1) {
                    bi = 2;
                } else {
                    push(blocks[0].left, blocks[0].right);
                }
                for (size_t j = 0; j < 2; j++) {
                    blocks[j] = blocks[j + 1];
                }
            }
        }
        if (bi == 1) {
            // entered once, one unused block
            push(blocks[1].left, blocks[1].right);

        } else if (bi == 2) {
            // entered many, two unused blocks
            auto& a = blocks[0];
            auto& b = blocks[1];

            if (a.size() == 1 || b.size() == 1) {
                // Adjust edge cases to prevent size-1 blocks
                // [1] [333] -> [22] [22]
                // [1] [22] -> [333]
                if (a.size() + b.size() == 4) {
                    a.right = a.left + 1;
                    b.left = b.right - 1;

                    push(a.left, a.right);
                    push(b.left, b.right);
                } else {
                    a.right = b.right;
                    push(a.left, a.right);
                }
            } else {
                push(a.left, a.right);
                push(b.left, b.right);
            }
        }
    }


}
}
