#ifndef TUDOCOMP_DRIVER_ALGORITHM_PARSER
#define TUDOCOMP_DRIVER_ALGORITHM_PARSER

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <istream>
#include <map>
#include <sstream>
#include <streambuf>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <memory>

#include <glog/logging.h>

#include <boost/variant.hpp>

#include <tudocomp/Env.hpp>
#include <tudocomp/Compressor.hpp>
#include <tudocomp/util.h>

namespace tudocomp_driver {
using namespace tudocomp;

    /*
        AlgorithmSpec ::= IDENT '(' [AlgorithmArg,]* [AlgorithmArg] ')'
        AlgorithmArg  ::= IDENT '=' string | string
    */

    struct AlgorithmArg;
    struct AlgorithmSpec {
        std::string name;
        std::vector<AlgorithmArg> args;

        inline std::string to_string() const;

        bool operator==(const AlgorithmSpec &other) const {
            return (name == other.name)
                && (args == other.args);
        }

        bool operator<(const AlgorithmSpec &other) const {
            if (name != other.name) return name < other.name;
            if (args != other.args) return args < other.args;
            return false;
        }
    };
    struct AlgorithmArg {
        std::string keyword;
        boost::variant<std::string, AlgorithmSpec> arg;

        template<class T>
        inline T get() {
            return boost::get<T>(arg);
        }

        inline std::string to_string() const;

        bool operator==(const AlgorithmArg &other) const {
            return (keyword == other.keyword)
                && (arg == other.arg);
        }

        bool operator<(const AlgorithmArg &other) const {
            if (keyword != other.keyword) return keyword < other.keyword;
            if (arg != other.arg) return arg < other.arg;
            return false;
        }
    };

    inline std::ostream& operator<<(std::ostream& os,
                                    const AlgorithmSpec& s) {
        os << s.to_string();
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os,
                                    const AlgorithmArg& s) {
        os << s.to_string();
        return os;
    }

    inline std::string AlgorithmSpec::to_string() const {
        auto& s = *this;
        std::stringstream os;
        os << s.name << "(";
        bool first = true;
        for(auto& arg : s.args) {
            if (!first) {
                os << ", ";
            }
            os << arg;
            first = false;
        }
        os << ")";
        return os.str();
    }

    inline std::string AlgorithmArg::to_string() const {
        auto& s = *this;
        std::stringstream os;
        if (s.keyword.size() > 0) {
            os << s.keyword << " = ";
        }
        os << s.arg;
        return os.str();
    }

    class Err {
        std::string m_reason;
        std::shared_ptr<Err> m_prev;
    public:
        inline Err(std::string reason): m_reason(reason) {
        }
        inline Err(const Err& prev, std::string reason): Err(reason) {
            m_prev = std::make_shared<Err>(prev);
        }
        inline std::string reason() {
            return m_reason;
        }
    };

    class Parser;

    template<class T>
    class Result {
        Parser* trail;
        boost::variant<T, Err> data;

    public:
        Result(Parser& parser, boost::variant<T, Err> data_):
            trail(&parser), data(data_) {}

        template<class A, class R>
        using Fun = std::function<Result<R> (A)>;

        template<class U>
        inline Result<U> and_then(Fun<T, U> f);

        inline Result<T> or_else(Fun<Err, T> f);

        inline T unwrap();

        inline Result<T> end_parse();

        inline bool is_ok() {
            struct visitor: public boost::static_visitor<bool> {
                bool operator()(T& ok) const {
                    return true;
                }
                bool operator()(Err& err) const {
                    return false;
                }
            };
            return boost::apply_visitor(visitor(), data);
        }

        inline bool is_err() {
            return !is_ok();
        }
    };

    class Parser {
        std::string m_input;
        size_t m_current_pos;

    public:
        inline Parser(const std::string& input): m_input(input), m_current_pos(0) {}
        inline Parser(const Parser& other) = delete;
        inline Parser(Parser&& other) = delete;

        inline std::string cursor() const {
            return m_input.substr(m_current_pos);
        }

        //inline void end_parse_or_error()

        inline void skip_whitespace() {
            for (; m_current_pos < m_input.size(); m_current_pos++) {
                if (m_input[m_current_pos] != ' ') {
                    return;
                }
            }
            return;
        }

        inline void skip(size_t i) {
            m_current_pos += i;
        }

        inline size_t cursor_pos() const {
            return m_current_pos;
        }

        inline const std::string& input() const {
            return m_input;
        }

        inline Result<std::string> parse_ident() {
            Parser& s = *this;

            s.skip_whitespace();

            auto valid_first = [](uint8_t c) {
                return (c == '_')
                    || (c >= 'a' && c <= 'z')
                    || (c >= 'A' && c <= 'Z');
            };
            auto valid_middle = [=](uint8_t c) {
                return valid_first(c)
                    || (c >= '0' && c <= '9');
            };

            size_t i = 0;
            if (i < s.cursor().size() && valid_first(s.cursor()[i])) {
                for (i = 1; i < s.cursor().size() && valid_middle(s.cursor()[i]); i++) {
                }
                auto r = s.cursor().substr(0, i);
                s.skip(i);
                return ok<std::string>(r);
            } else {
                return err<std::string>("Expected an identifier");
            }
        }

        inline Result<uint8_t> parse_char(uint8_t chr) {
            Parser& s = *this;

            s.skip_whitespace();

            if (s.cursor().size() > 0 && uint8_t(s.cursor()[0]) == chr) {
                s.skip(1);
                return ok<uint8_t>(chr);
            } else {
                return err<uint8_t>(std::string("Expected char '")
                    + char(chr) + "'" + ", found '"
                    + s.cursor()[0] + "'");
            }
        }

        inline Result<std::string> parse_number() {
            Parser& s = *this;

            s.skip_whitespace();

            auto valid = [=](uint8_t c) {
                return c >= '0' && c <= '9';
            };

            size_t i = 0;
            if (i < s.cursor().size() && valid(s.cursor()[i])) {
                for (i = 1; i < s.cursor().size() && valid(s.cursor()[i]); i++) {
                }
                auto r = s.cursor().substr(0, i);
                s.skip(i);
                return ok<std::string>(r);
            } else {
                return err<std::string>("Expected an number");
            }
        }

        template<class T>
        inline Result<T> ok(T t) {
            return Result<T> {
                *this,
                t,
            };
        }

        template<class T>
        inline Result<T> err(std::string msg) {
            return Result<T> {
                *this,
                Err { msg },
            };
        }

        template<class T>
        inline Result<T> err(Err msg) {
            return Result<T> {
                *this,
                msg,
            };
        }

        inline Result<AlgorithmArg> parse_arg(const std::string& keyword = "") {
            Parser& p = *this;

            return p.parse_ident().and_then<AlgorithmArg>([&](const std::string& arg_ident) {
                // "ident ..." case

                if (keyword == "") {
                    auto r = p.parse_char('=').and_then<AlgorithmArg>([&](uint8_t chr) {
                        // "ident = ..." case
                        return p.parse_arg(arg_ident);
                    });

                    if (r.is_ok()) {
                        return r;
                    }
                }

                return p.parse_args().and_then<AlgorithmArg>([&](std::vector<AlgorithmArg> args) {
                    // "ident(...) ..." case
                    return p.ok(AlgorithmArg {
                        keyword,
                        AlgorithmSpec {
                            arg_ident,
                            args
                        }
                    });
                }).or_else([&](Err err) {
                    // "ident ..." fallback case
                    return p.ok<AlgorithmArg>(AlgorithmArg {
                        keyword,
                        std::string(arg_ident)
                    });
                });
            }).or_else([&](Err err) {
                return p.parse_number().and_then<AlgorithmArg>([&](const std::string& n) {
                    // "num ..." case
                    return p.ok<AlgorithmArg>(AlgorithmArg {
                        keyword,
                        std::string(n)
                    });
                });
            });
        }

        inline Result<std::vector<AlgorithmArg>> parse_args() {
            Parser& p = *this;

            return p.parse_char('(').and_then<std::vector<AlgorithmArg>>([&](uint8_t chr) {
                // Parse arguments here

                std::vector<AlgorithmArg> args;

                while (true) {
                    auto arg = p.parse_arg();
                    if (arg.is_ok()) {
                        args.push_back(arg.unwrap());
                        if (p.parse_char(',').is_err()) {
                            break;
                        }
                    } else {
                        break;
                    }
                }

                return p.parse_char(')').and_then<std::vector<AlgorithmArg>>([&](uint8_t chr) {
                    return p.ok(args);
                });
            });
        }

        inline Result<AlgorithmSpec> parse() {
            Parser& p = *this;

            return p.parse_ident().and_then<AlgorithmSpec>([&](const std::string& ident) {
                return p.parse_args().and_then<AlgorithmSpec>([&](std::vector<AlgorithmArg> args) {
                    return p.ok(AlgorithmSpec {
                        ident,
                        args
                    });
                });
            }).end_parse();
        }

    };

    template<class T>
    inline T Result<T>::unwrap() {
        struct visitor: public boost::static_visitor<T> {
            const Parser* m_trail;

            visitor(Parser* trail): m_trail(trail) {
            }

            T operator()(T& ok) const {
                return ok;
            }
            T operator()(Err& err) const {
                std::stringstream ss;

                ss << "\nParse error at #" << int(m_trail->cursor_pos()) << ":\n";
                ss << m_trail->input() << "\n";
                ss << std::setw(m_trail->cursor_pos()) << "";
                ss << "^\n";
                ss << err.reason() << "\n";

                throw std::runtime_error(ss.str());
            }
        };
        return boost::apply_visitor(visitor(trail), data);
    }

    template<class T>
    template<class U>
    inline Result<U> Result<T>::and_then(Fun<T, U> f) {
        struct visitor: public boost::static_visitor<Result<U>> {
            // insert constructor here
            Fun<T, U> m_f;
            Parser* m_trail;

            visitor(Fun<T, U> f, Parser* trail): m_f(f), m_trail(trail) {
            }

            Result<U> operator()(T& ok) const {
                return m_f(ok);
            }
            Result<U> operator()(Err& err) const {
                return m_trail->err<U>(err);
            }
        };
        return boost::apply_visitor(visitor(f, trail), data);
    }

    template<class T>
    inline Result<T> Result<T>::or_else(Fun<Err, T> f) {
        struct visitor: public boost::static_visitor<Result<T>> {
            // insert constructor here
            Fun<Err, T> m_f;
            Parser* m_trail;

            visitor(Fun<Err, T> f, Parser* trail): m_f(f), m_trail(trail) {
            }

            Result<T> operator()(T& ok) const {
                return m_trail->ok<T>(ok);
            }
            Result<T> operator()(Err& err) const {
                return m_f(err);
            }
        };
        return boost::apply_visitor(visitor(f, trail), data);
    }

    template<class T>
    inline Result<T> Result<T>::end_parse() {
        return this->and_then<T>([&](T _t) {
            Parser& p = *trail;

            p.skip_whitespace();

            if (p.cursor() == "") {
                return *this;
            } else {
                return p.err<T>("Expected end of input");
            }
        });
    }
}

#endif