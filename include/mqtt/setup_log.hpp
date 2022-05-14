// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_SETUP_LOG_HPP)
#define MQTT_SETUP_LOG_HPP

// This is an example implementation for logging setup.
// If your code doesn't use Boost.Log then you can use the setup_log() directly.
// setup_log() provides a typical console logging setup.
// If you want to use existing Boost.Log related code with mqtt_cpp,
// then you can write your own logging setup code.
// setup_log() could be  a good reference for your own logging setup code.

#include <mqtt/namespace.hpp>
#include <mqtt/log.hpp>

#if defined(MQTT_USE_LOG)

#include <mqtt/move.hpp>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

#endif // defined(MQTT_USE_LOG)

namespace MQTT_NS {

#if defined(MQTT_USE_LOG)

static constexpr char const* log_color_table[] {
    "\033[0m", // trace
    "\033[36m", // debug
    "\033[32m", // info
    "\033[33m", // warning
    "\033[35m", // error
    "\033[31m", // fatal
};

/**
 * @brief Setup logging
 * @param threshold
 *        Set threshold severity_level by channel
 *        If the log severity_level >= threshold then log message outputs.
 */
inline
void setup_log(std::map<std::string, severity_level> threshold) {
    // https://www.boost.org/doc/libs/1_73_0/libs/log/doc/html/log/tutorial/advanced_filtering.html

    auto fmt =
        [](boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
            // Timestamp custom formatting example
            if (auto v = boost::log::extract<boost::posix_time::ptime>("TimeStamp", rec)) {
                strm.imbue(
                    std::locale(
                        strm.getloc(),
                        // https://www.boost.org/doc/html/date_time/date_time_io.html#date_time.format_flags
                        new boost::posix_time::time_facet("%H:%M:%s") // ownership is moved here
                    )
                );
                strm << v.get() << " ";
            }
            // ThreadID example
            if (auto v = boost::log::extract<boost::log::thread_id>("ThreadID", rec)) {
                strm << "T:" << v.get() << " ";
            }
            // Adjust severity length example
            if (auto v = boost::log::extract<severity_level>("Severity", rec)) {
                strm << log_color_table[static_cast<std::size_t>(v.get())];
                strm << "S:" << std::setw(7) << std::left << v.get() << " ";
            }
            if (auto v = boost::log::extract<channel>("Channel", rec)) {
                strm << "C:" << std::setw(5) << std::left << v.get() << " ";
            }
            // Shorten file path example
            if (auto v = boost::log::extract<std::string>("MqttFile", rec)) {
                strm << boost::filesystem::path(v.get()).filename().string() << ":";
            }
            if (auto v = boost::log::extract<unsigned int>("MqttLine", rec)) {
                strm << v.get() << " ";
            }
            if (auto v = boost::log::extract<void const*>("MqttAddress", rec)) {
                strm << "A:" << v.get() << " ";
            }

#if 0 // function is ofthen noisy
            if (auto v = boost::log::extract<std::string>("MqttFunction", rec)) {
                strm << v << ":";
            }
#endif
            strm << rec[boost::log::expressions::smessage];
            strm << "\033[0m";
        };

    // https://www.boost.org/doc/libs/1_73_0/libs/log/doc/html/log/tutorial/sinks.html
    boost::shared_ptr<std::ostream> stream(&std::clog, boost::null_deleter());

    using text_sink = boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>;
    auto sink = boost::make_shared<text_sink>();
    sink->locked_backend()->add_stream(stream);
    sink->set_formatter(fmt);

    auto fil =
        [threshold = force_move(threshold)]
        (boost::log::attribute_value_set const& avs) {
            {
                // For mqtt
                auto chan = boost::log::extract<channel>("Channel", avs);
                auto sev = boost::log::extract<severity_level>("Severity", avs);
                if (chan && sev) {
                    auto it = threshold.find(chan.get());
                    if (it == threshold.end()) return false;
                    return sev.get() >= it->second;
                }
            }
            return true;
        };

    boost::log::core::get()->set_filter(fil);
    boost::log::core::get()->add_sink(sink);

    boost::log::add_common_attributes();
}

/**
 * @brief Setup logging
 * @param threshold
 *        Set threshold severity_level for all channels
 *        If the log severity_level >= threshold then log message outputs.
 */
inline
void setup_log(severity_level threshold = severity_level::warning) {
    setup_log(
        {
            { "mqtt_api", threshold },
            { "mqtt_cb", threshold },
            { "mqtt_impl", threshold },
            { "mqtt_broker", threshold },
            { "mqtt_test", threshold },
        }
    );
}

#else  // defined(MQTT_USE_LOG)

template <typename... Params>
void setup_log(Params&&...) {}

#endif // defined(MQTT_USE_LOG)

} // namespace MQTT_NS

#endif // MQTT_SETUP_LOG_HPP
