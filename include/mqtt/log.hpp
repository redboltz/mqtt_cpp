// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_LOG_HPP)
#define MQTT_LOG_HPP

#include <cstddef>
#include <ostream>
#include <string>

#if defined(MQTT_USE_LOG)

#include <boost/log/core.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/attributes/scoped_attribute.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/preprocessor/if.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/greater_equal.hpp>

#endif // defined(MQTT_USE_LOG)

#include <mqtt/namespace.hpp>

namespace MQTT_NS {

struct channel : std::string {
    using std::string::string;
};

enum class severity_level {
    trace,
    debug,
    info,
    warning,
    error,
    fatal
};

inline std::ostream& operator<<(std::ostream& o, severity_level sev) {
    constexpr char const* const str[] {
        "trace",
        "debug",
        "info",
        "warning",
        "error",
        "fatal"
    };
    o << str[static_cast<std::size_t>(sev)];
    return o;
}

namespace detail {

struct null_log {
    template <typename... Params>
    constexpr null_log(Params&&...) {}
};

template <typename T>
inline constexpr null_log const& operator<<(null_log const& o, T const&) { return o; }

} // namespace detail

#if defined(MQTT_USE_LOG)

// template arguments are defined in MQTT_NS
// filter and formatter can distinguish mqtt_cpp's channel and severity by their types
using global_logger_t = boost::log::sources::severity_channel_logger<severity_level, channel>;
inline global_logger_t& logger() {
    thread_local global_logger_t l;
    return l;
}

// Normal attributes
BOOST_LOG_ATTRIBUTE_KEYWORD(file, "MqttFile", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line, "MqttLine", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(function, "MqttFunction", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(address, "MqttAddress", void const*)


// Take any filterable parameters (FP)
#define MQTT_LOG_FP(chan, sev)                                          \
    BOOST_LOG_STREAM_CHANNEL_SEV(MQTT_NS::logger(), MQTT_NS::channel(chan), sev) \
    << boost::log::add_value(MQTT_NS::file, __FILE__)                   \
    << boost::log::add_value(MQTT_NS::line, __LINE__)                   \
    << boost::log::add_value(MQTT_NS::function, BOOST_CURRENT_FUNCTION)

#define MQTT_GET_LOG_SEV_NUM(lv) BOOST_PP_CAT(MQTT_, lv)

// Use can set preprocessor macro MQTT_LOG_SEV.
// For example, -DMQTT_LOG_SEV=info, greater or equal to info log is generated at
// compiling time.

#if !defined(MQTT_LOG_SEV)
#define MQTT_LOG_SEV trace
#endif // !defined(MQTT_LOG_SEV)

#define MQTT_trace   0
#define MQTT_debug   1
#define MQTT_info    2
#define MQTT_warning 3
#define MQTT_error   4
#define MQTT_fatal   5

// User can define custom MQTT_LOG implementation
// By default MQTT_LOG_FP is used


#if !defined(MQTT_LOG)

#define MQTT_LOG(chan, sev)                                             \
    BOOST_PP_IF(                                                        \
        BOOST_PP_GREATER_EQUAL(MQTT_GET_LOG_SEV_NUM(sev), MQTT_GET_LOG_SEV_NUM(MQTT_LOG_SEV)), \
        MQTT_LOG_FP(chan, MQTT_NS::severity_level::sev),                \
        MQTT_NS::detail::null_log(chan, MQTT_NS::severity_level::sev)   \
    )

#endif // !defined(MQTT_LOG)

#if !defined(MQTT_ADD_VALUE)

#define MQTT_ADD_VALUE(name, val) boost::log::add_value((MQTT_NS::name), (val))

#endif // !defined(MQTT_ADD_VALUE)

#else  // defined(MQTT_USE_LOG)

#if !defined(MQTT_LOG)

#define MQTT_LOG(chan, sev) MQTT_NS::detail::null_log(chan, MQTT_NS::severity_level::sev)

#endif // !defined(MQTT_LOG)

#if !defined(MQTT_ADD_VALUE)

#define MQTT_ADD_VALUE(name, val) val

#endif // !defined(MQTT_ADD_VALUE)

#endif // defined(MQTT_USE_LOG)

} // namespace MQTT_NS

#endif // MQTT_LOG_HPP
