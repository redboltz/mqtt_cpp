// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_APPLY_HPP)
#define MQTT_APPLY_HPP

#include <tuple>

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

#if cplusplus >= 201703L

template <typename F, typename Tuple>
decltype(auto) apply(F&& f, Tuple&& t) {
    return std::apply(std::forward<F>(f), std::forward<Tuple>(t));
}

#else  // __cplusplus >= 201703L

namespace detail {

template < typename F, typename Tuple, std::size_t... Idx>
constexpr decltype(auto)
apply_impl(F&& f, Tuple&& t, std::index_sequence<Idx...>) {
    return static_cast<F&&>(f)(std::get<Idx>(static_cast<Tuple&&>(t))...);
}

} // namespace detail

template <typename F, typename Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t) {
    return
        detail::apply_impl(
            static_cast<F&&>(f),
            static_cast<Tuple&&>(t),
            std::make_index_sequence<
                std::tuple_size<
                    std::remove_reference_t<Tuple>
                >::value
            >{}
        );
}


#endif // __cplusplus >= 201703L

} // namespace MQTT_NS

#endif // MQTT_APPLY_HPP
