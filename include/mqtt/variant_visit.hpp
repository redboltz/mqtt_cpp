// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_VARIANT_VISIT_HPP)
#define MQTT_VARIANT_VISIT_HPP

#include <ostream>

#include <mqtt/namespace.hpp>
#include <mqtt/variant.hpp>
#include <mqtt/visitor_util.hpp>

namespace MQTT_NS {

#if defined(MQTT_STD_VARIANT)

template<typename... Params>
std::ostream& operator<<(std::ostream& os, variant<Params...> const& v) {
    MQTT_NS::visit(
        make_lambda_visitor(
            [&os] (auto const& e) {
                os << e;
            }
        ), v
    );
    return os;
}

#endif // defined(MQTT_USE_STD_VARIANT)

} // namespace MQTT_NS

#endif // MQTT_VARIANT_VISIT_HPP
