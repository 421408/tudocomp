#pragma once

#include <cstddef>
#include <limits>
#include <utility>

#include <set>
#include <vector>

namespace tdc {
namespace is {

/// \cond INTERNAL

// declarations

template<typename T, T Current, typename Seq> struct _max;
template<typename T, T Value, typename Seq> struct _contains;
template<typename T, T Value, typename Seq> struct _prepend;
template<typename T, typename Compare, T Value, typename Seq> struct _insert;
template<typename T, typename Compare, typename Seq> struct _sort;

/// \endcond

/// \brief Finds the maximum in an integer sequence.
///
/// \tparam T the integer type
/// \tparam Seq the sequence to search
/// \return the maximum value in \c Seq.
template<typename T, typename Seq>
constexpr T max() {
    return _max<T, std::numeric_limits<T>::min(), Seq>::value;
};

/// \brief Finds the maximum in an index sequence.
///
/// \tparam Seq the sequence to search
/// \return the maximum value in \c Seq.
template<typename Seq>
constexpr size_t max_idx() {
    return _max<size_t, 0, Seq>::value;
};

/// \brief Reports whether a value is contained in an integer sequence.
///
/// \tparam T the integer type
/// \tparam Value the value to look for
/// \tparam Seq the sequence to search
/// \return \c true iff \c Value is contained in \c Seq.
template<typename T, T Value, typename Seq>
constexpr bool contains() {
    return _contains<T, Value, Seq>::value;
};

/// \brief Reports whether a value is contained in an index sequence.
///
/// \tparam Value the value to look for
/// \tparam Seq the sequence to search
/// \return \c true iff \c Value is contained in \c Seq.
template<size_t Value, typename Seq>
constexpr bool contains_idx() {
    return _contains<size_t, Value, Seq>::value;
};

/// \brief Prepends a value to an integer sequence.
///
/// \tparam T the integer type
/// \tparam Value the value to prepend
/// \tparam Seq the sequence to prepend to
template<typename T, T Value, typename Seq>
using prepend = typename _prepend<T, Value, Seq>::seq;

/// \brief Ascending comparator for insertion sort.
///
/// The comparator returns \c true iff the first value is less than or equal
/// to the second using the \c <= operator.
struct ascending {
    template<typename T, T A, T B>
    static constexpr bool compare() {
        return A <= B;
    }
};

/// \brief Descending comparator for insertion sort.
///
/// The comparator returns \c true iff the first value is greater than or equal
/// to the second using the \c >= operator.
struct descending {
    template<typename T, T A, T B>
    static constexpr bool compare() {
        return A >= B;
    }
};

/// \brief Inserts an integer value into a sorted sequence while retaining the
///        ordering.
///
/// \tparam T the integer type
/// \tparam Value the value to insert
/// \tparam Seq the sorted integer sequence to insert into
/// \tparam Compare the value comparator. Must implement a special \c compare
///         function. See \ref ascending and \ref descending for examples.
template<typename T, T Value, typename Seq, typename Compare = ascending>
using insert = typename _insert<T, Compare, Value, Seq>::seq;

/// \brief Sorts an integer sequence using insertion sort.
///
/// \tparam T the integer type
/// \tparam Seq the sequence to sort
/// \tparam Compare the value comparator. Must implement a special \c compare
///         function. See \ref ascending and \ref descending for examples.
template<typename T, typename Seq, typename Compare = ascending>
using sort = typename _sort<T, Compare, Seq>::seq;

/// \brief Sorts an index sequence using insertion sort.
///
/// \tparam Seq the sequence to sort
/// \tparam Compare the value comparator. Must implement a
///         <code>static constexpr bool compare(T a, T b)</code> function that
///         returns \c true iff a comes before b in the ordering.
template<typename Seq, typename Compare = ascending>
using sort_idx = typename _sort<size_t, Compare, Seq>::seq;

/// \cond INTERNAL

// implementations

// max - trivial case (sequence is empty)
template<typename T, T Current>
struct _max<T, Current, std::integer_sequence<T>> {
    static constexpr T value = Current;
};

// max - SFINAE case 1: Head > Current
template<typename T, T Current, T Head, T... Tail>
constexpr typename std::enable_if<(Head > Current), T>::type _max_f() {
    return _max<T, Head, std::integer_sequence<T, Tail...>>::value;
}

// max - SFINAE case 2: Head <= Current
template<typename T, T Current, T Head, T... Tail>
constexpr typename std::enable_if<(Head <= Current), T>::type _max_f() {
    return _max<T, Current, std::integer_sequence<T, Tail...>>::value;
}

template<typename T, T Current, T... Seq>
struct _max<T, Current, std::integer_sequence<T, Seq...>> {
    static constexpr T value = _max_f<T, Current, Seq...>();
};

// contains - trivial case (sequence is empty)
template<typename T, T Value>
struct _contains<T, Value, std::integer_sequence<T>> {
    static constexpr bool value = false;
};

// contains - SFINAE trivial case: Head equals searched Value
template<typename T, T Value, T Head, T... Tail>
constexpr typename std::enable_if<Value == Head, bool>::type _contains_f() {
    return true;
}

// contains - SFINAE recursive case: search for Value in remainder
template<typename T, T Value, T Head, T... Tail>
constexpr typename std::enable_if<Value != Head, bool>::type _contains_f() {
    return contains<T, Value, std::integer_sequence<T, Tail...>>();
}

// contains - standard case (sequence is not empty)
template<typename T, T Value, T... Seq>
struct _contains<T, Value, std::integer_sequence<T, Seq...>> {
    static constexpr bool value = _contains_f<T, Value, Seq...>();
};

// prepend - impl
template<typename T, T Value, T... Seq>
struct _prepend<T, Value, std::integer_sequence<T, Seq...>> {
    using seq = std::integer_sequence<T, Value, Seq...>;
};

// insert operation - case 1: Value <= Head
// simply prepend Value to remaining sequence
template<typename T, typename Compare, T Value, T Head, T... Tail>
constexpr typename std::enable_if<Compare::template compare<T, Value, Head>(),
    std::integer_sequence<T, Value, Head, Tail...>
    >::type _insert_op();

// insert operation - case 2: Value > Head
// prepend Head to recursive application
template<typename T, typename Compare, T Value, T Head, T... Tail>
constexpr typename std::enable_if<!Compare::template compare<T, Value, Head>(),
    prepend<T, Head, insert<T, Value, std::integer_sequence<T, Tail...>, Compare>>
    >::type _insert_op();

// insert - trivial case (sequence is empty)
template<typename T, typename Compare, T Value>
struct _insert<T, Compare, Value, std::integer_sequence<T>> {
    using seq = std::integer_sequence<T, Value>;
};

// insert - recursive case
// prepend value iff Head is less or equal
template<typename T, typename Compare, T Value, T Head, T... Tail>
struct _insert<T, Compare, Value, std::integer_sequence<T, Head, Tail...>> {
    using seq = decltype(_insert_op<T, Compare, Value, Head, Tail...>());
};

// insertion sort - trivial case
// empty sequence
template<typename T, typename Compare>
struct _sort<T, Compare, std::integer_sequence<T>> {
    using seq = std::integer_sequence<T>;
};

// insertion sort - recursive case
// insert next value into sequence
template<typename T, typename Compare, T Head, T... Tail>
struct _sort<T, Compare, std::integer_sequence<T, Head, Tail...>> {
    using seq = insert<T, Head,
        sort<T, std::integer_sequence<T, Tail...>, Compare>,
        Compare>;
};

/// \endcond

/// \cond INTERNAL
template<typename T>
inline void _to_set(std::set<T>& set, std::integer_sequence<T>) {
    // done
}

template<typename T, T Head, T... Tail>
inline void _to_set(std::set<T>& set,
    std::integer_sequence<T, Head, Tail...>) {

    // add head and recurse
    set.emplace(Head);
    _to_set(set, std::integer_sequence<T, Tail...>());
}

template<typename T>
inline void _to_vector(std::vector<T>& v, std::integer_sequence<T>) {
    // done
}

template<typename T, T Head, T... Tail>
inline void _to_vector(std::vector<T>& v,
    std::integer_sequence<T, Head, Tail...>) {

    // add head and recurse
    v.emplace_back(Head);
    _to_vector(v, std::integer_sequence<T, Tail...>());
}
/// \endcond

/// \brief Creates a set from an integer sequence.
///
/// \tparam T the integer type
/// \tparam Seq the integer sequence
template<typename T, T... Seq>
inline std::set<T> to_set(std::integer_sequence<T, Seq...>) {
    std::set<T> set;
    _to_set(set, std::integer_sequence<T, Seq...>());
    return set;
}

/// \brief Creates a vector from an integer sequence.
///
/// \tparam T the integer type
/// \tparam Seq the integer sequence
template<typename T, T... Seq>
inline std::vector<T> to_vector(std::integer_sequence<T, Seq...>) {
    std::vector<T> v;
    v.reserve(sizeof...(Seq));
    _to_vector(v, std::integer_sequence<T, Seq...>());
    return v;
}

}} //ns
