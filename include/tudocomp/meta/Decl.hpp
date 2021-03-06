#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <tudocomp/meta/ast/Node.hpp>
#include <tudocomp/meta/ast/List.hpp>
#include <tudocomp/meta/ast/Value.hpp>
#include <tudocomp/meta/ast/Object.hpp>
#include <tudocomp/meta/ast/TypeConversion.hpp>

#include <tudocomp/meta/TypeDesc.hpp>

namespace tdc {
namespace meta {

/// \brief Error type for declaration related errors.
class DeclError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr TypeDesc untyped("$");

/// \brief Represents an algorithm declaration.
class Decl {
public:
    /// \brief Represents a declared parameter for an algorithm.
    class Param {
    public:
        /// \brief Enumeration of parameter kinds.
        enum Kind {
            primitive, /// a primitive value
            complex,   /// a complex value
            bound,     /// a type-bound strategy, which appears in signatures
            unbound    /// an unbound strategy, which cannot appear in signatures
        };

    private:
        std::string m_name;
        std::string m_desc;

        Kind m_kind;        // if not primitive, type needs to be set
        bool m_list;        // if true, value/type flags account for list items
        TypeDesc m_type;    // only valid if non-primitive

        // default value
        ast::NodePtr<> m_default;

    public:
        /// \brief Main constructor.
        /// \param name the parameter name
        /// \param desc the parameter's description
        /// \param primitive if \c true, the parameter accepts string values,
        ///                  otherwise it accepts a sub algorithm
        /// \param list if \c true, the parameter accepts a list of values
        /// \param type if the parameter is non-primitive, sub algorithms must
        ///             match this type
        inline Param(
            const std::string& name,
            const std::string& desc = "",
            Kind kind = Kind::primitive,
            bool list = false,
            const TypeDesc& type = TypeDesc(),
            ast::NodePtr<> defv = ast::NodePtr<>())
            : m_name(name),
              m_desc(desc),
              m_kind(kind),
              m_list(list),
              m_default(defv) {

            if(has_type() && !type.valid()) {
                throw DeclError("typed parameters must have a type");
            }

            if(!has_type() && type.valid()) {
                throw DeclError("untyped parameters must not have a type");
            }

            m_type = has_type() ? type : untyped;

            // sanity checks on default value
            if(defv) {
                if(list) {
                    auto list_value = ast::convert<ast::List>(defv,
                        "default value type mismatch");

                    // check each list item
                    if(is_primitive()) {
                        for(auto& item : list_value->items()) {
                            ast::convert<ast::Value>(item,
                                "default list item type mismatch");
                        }
                    } else {
                        for(auto& item : list_value->items()) {
                            ast::convert<ast::Object>(item,
                                "default list item type mismatch");
                        }
                    }
                } else if(is_primitive()) {
                    ast::convert<ast::Value>(defv,
                        "default value type mismatch");
                } else {
                    ast::convert<ast::Object>(defv,
                        "default value type mismatch");
                }
            }
        }

        /// \brief Copy constructor.
        /// \param other the object to copy
        inline Param(const Param& other)
            : m_name(other.m_name),
              m_desc(other.m_desc),
              m_kind(other.m_kind),
              m_list(other.m_list),
              m_type(other.m_type),
              m_default(other.m_default) {
        }

        /// \brief Move constructor.
        /// \param other the object to move
        inline Param(Param&& other)
            : m_name(std::move(other.m_name)),
              m_desc(std::move(other.m_desc)),
              m_kind(other.m_kind),
              m_list(other.m_list),
              m_type(std::move(other.m_type)),
              m_default(std::move(other.m_default)) {
        }

        inline const std::string& name() const { return m_name; }
        inline const std::string& desc() const { return m_desc; }
        inline Kind kind() const { return m_kind; }
        inline bool is_list() const { return m_list; }
        inline const TypeDesc& type() const { return m_type; }

        inline bool is_primitive() const { return m_kind == primitive; }
        inline bool has_type() const {
            return m_kind == bound || m_kind == unbound;
        }

        inline ast::NodePtr<> default_value() const {
            return m_default;
        }

        /// \brief Returns a string representation of the declaration.
        /// \return a string representation of the declaration
        inline const std::string str() const {
            return m_name + " : " +
                (m_list ? "[" + m_type.name() + "]" : m_type.name());
        }
    };

private:
    std::string m_name;
    TypeDesc    m_type;
    std::string m_desc;
    std::vector<Param> m_params;

public:
    /// \brief Main constructor.
    /// \param name the algorithm's name
    /// \param type the algorithm's type
    /// \param desc a brief documentaton of the algorithm
    inline Decl(
        const std::string& name,
        const TypeDesc&    type,
        const std::string& desc = "")
        : m_name(name), m_type(type), m_desc(desc) {
    }

    /// \brief Copy constructor.
    /// \param other the object to copy
    inline Decl(const Decl& other)
        : m_name(other.m_name),
          m_type(other.m_type),
          m_desc(other.m_desc),
          m_params(other.m_params) {
    }

    /// \brief Move constructor.
    /// \param other the object to move
    inline Decl(Decl&& other)
        : m_name(std::move(other.m_name)),
          m_type(std::move(other.m_type)),
          m_desc(std::move(other.m_desc)),
          m_params(std::move(other.m_params)) {
    }

    /// \brief Adds a parameter to the declaration.
    /// \param p the parameter to add
    inline void add_param(Param&& p) {
        m_params.emplace_back(std::move(p));
    }

    inline const std::string& name() const { return m_name; }
    inline const TypeDesc&    type() const { return m_type; }
    inline const std::string& desc() const { return m_desc; }
    inline const std::vector<Param>& params() const {
        return m_params;
    }

    /// \brief Returns a string representation of the declaration.
    /// \return a string representation of the declaration
    inline std::string str() const {
        std::stringstream ss;
        ss << m_name << "(";

        size_t i = 0;
        for(auto& param : m_params) {
            ss << param.str();
            if(++i < m_params.size()) ss << ", ";
        }

        ss << ") : " << m_type.name();
        return ss.str();
    }
};

}} //ns
