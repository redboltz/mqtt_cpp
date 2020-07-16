// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_LOG_HPP)
#define MQTT_LOG_HPP

#include <tuple>

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


namespace MQTT_NS {

namespace log = boost::log;

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

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, log::sources::severity_channel_logger_mt<severity_level>);

// Scoped attributes
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "MqttSeverity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "MqttChannel", std::string)

// Normal attributes
BOOST_LOG_ATTRIBUTE_KEYWORD(file, "MqttFile", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(line, "MqttLine", unsigned int)
BOOST_LOG_ATTRIBUTE_KEYWORD(function, "MqttFunction", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(address, "MqttAddress", void const*)


template <typename Logger, typename Name, typename Attr, typename... Ts>
struct scoped_attrs_cond : scoped_attrs_cond<Logger, Ts...> {
    scoped_attrs_cond(Logger& l, Name&& n, Attr&& a, Ts&&... ts)
        : scoped_attrs_cond<Logger, Ts...> { l, std::forward<Ts>(ts)... }
        , sla { l, std::forward<Name>(n), log::attributes::make_constant(std::forward<Attr>(a)) }
    {
    }

    log::aux::scoped_logger_attribute<Logger> sla;
};

template <typename Logger, typename Name, typename Attr>
struct scoped_attrs_cond<Logger, Name, Attr> {
    scoped_attrs_cond(Logger& l, Name&& n, Attr&& a)
        : guard { mutex() }, sla { l, std::forward<Name>(n), log::attributes::make_constant(std::forward<Attr>(a)) }
    {
    }
    operator bool() const {
        return false;
    }
    static std::mutex& mutex() {
        static std::mutex mtx;
        return mtx;
    }
    std::unique_lock<std::mutex> guard;
    log::aux::scoped_logger_attribute<Logger> sla;
};

template <typename Logger, typename... Ts>
auto make_scoped_attrs_cond(Logger& l, Ts&&... ts) {
    return scoped_attrs_cond<Logger, Ts...> { l, std::forward<Ts>(ts)... };
}

// Take any filterable parameters (FP)
#define MQTT_LOG_FP(...) \
    if (auto mqtt_scoped_attrs = make_scoped_attrs_cond(logger::get(), __VA_ARGS__) \
    ) { \
        (void)mqtt_scoped_attrs; \
    } \
    else \
        BOOST_LOG(logger::get()) \
            << log::add_value(file, __FILE__) \
            << log::add_value(line, __LINE__) \
            << log::add_value(function, BOOST_CURRENT_FUNCTION)


namespace detail {

struct null_log {
    template <typename... Params>
    constexpr null_log(Params&&...) {}
};

template <typename T>
inline constexpr null_log const& operator<<(null_log const& o, T const&) { return o; }

} // namespace detail

#if defined(MQTT_DISABLE_LOG)

#define MQTT_LOG(chan, sev) detail::null_log(chan, severity_level::sev)

#else  // defined(MQTT_DISABLE_LOG)

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
        MQTT_LOG_FP("MqttChannel", chan, "MqttSeverity", severity_level::sev), \
        MQTT_NS::detail::null_log(chan, severity_level::sev)            \
    )

#endif // !defined(MQTT_LOG)

#endif // defined(MQTT_DISABLE_LOG)

} // namespace MQTT_NS

#endif // MQTT_LOG_HPP
