#pragma once

#include <memory>
#include <vector>

#include <tudocomp/io.hpp>
#include <tudocomp/meta/Registry.hpp>
#include <tudocomp/Compressor.hpp>

namespace tdc {

class ChainSyntaxPreprocessor {
public:
    static inline std::string preprocess(const std::string& str) {
        // replace substrings separated by ':' by chain invokations
        size_t pos = str.find(':');
        if(pos != std::string::npos) {
            return std::string("chain(") +
                    str.substr(0, pos) + ", " +
                    preprocess(str.substr(pos+1)) + ")";
        } else {
            return str;
        }
    }
};

class ChainCompressor: public Compressor {
public:
    inline static Meta meta() {
        Meta m(Compressor::type_desc(), "chain",
            "Executes two compressors consecutively, passing the first "
            "compressors output to the input of the second.");
        m.param("first", "The first compressor.")
            .unbound_strategy(Compressor::type_desc());
        m.param("second", "The second compressor.")
            .unbound_strategy(Compressor::type_desc());
        return m;
    }

    /// No default construction allowed
    inline ChainCompressor() = delete;

    /// Construct the class with an environment and the algorithms to chain.
    inline ChainCompressor(Config&& cfg):
        Compressor(std::move(cfg)) {}

    template<class F>
    inline void chain(Input& input, Output& output, bool reverse, F f) {
        string_ref first_algo = "first";
        string_ref second_algo = "second";

        if (reverse) {
            std::swap(first_algo, second_algo);
        }

        auto run = [&](Input& i, Output& o, string_ref option) {
            auto option_value = config().param(option);

            auto compressor = Registry::of<Compressor>().select(
                meta::ast::convert<meta::ast::Object>(option_value.ast()));

            DVLOG(1) << "dynamic creation of " << compressor.decl()->name();
            f(i, o, *compressor);
        };

        std::vector<uint8_t> between_buf;
        {
            Output between(between_buf);
            run(input, between, first_algo);
        }
        DLOG(INFO) << "Buffer between chain: " << vec_to_debug_string(between_buf);
        {
            Input between(between_buf);
            run(between, output, second_algo);
        }
    }

    /// Compress `inp` into `out`.
    ///
    /// \param input The input stream.
    /// \param output The output stream.
    inline virtual void compress(Input& input, Output& output) override final {
        chain(input, output, false, [](Input& i,
                                       Output& o,
                                       Compressor& c) {
            c.compress(i, o);
        });
    }

    /// Decompress `inp` into `out`.
    ///
    /// \param input The input stream.
    /// \param output The output stream.
    inline virtual void decompress(Input& input, Output& output) override final {
        chain(input, output, true, [](Input& i,
                                      Output& o,
                                      Compressor& c) {
            c.decompress(i, o);
        });
    }
};

}
