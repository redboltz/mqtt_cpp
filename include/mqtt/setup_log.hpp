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


#include <mqtt/log.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>

namespace MQTT_NS {


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
            // ThreadID indexed example
            if (auto v = boost::log::extract<boost::log::thread_id>("ThreadID", rec)) {
                static std::map<boost::log::thread_id, std::size_t> m;
                auto it = m.find(v.get());
                if (it == m.end()) {
                    auto r = m.emplace(v.get(), m.size());
                    BOOST_ASSERT(r.second);
                    it = r.first;
                }
                strm << "TID[" << it->second << "] ";
            }
            // Adjust severity length example
            if (auto v = boost::log::extract<MQTT_NS::severity_level>("MqttSeverity", rec)) {
                strm << "SEV[" << std::setw(7) << std::left << v.get() << "] ";
            }
            if (auto v = boost::log::extract<boost::log::trivial::severity_level>("Severity", rec)) {
                strm << "SEV[" << v.get() << "] ";
            }
            if (auto v = boost::log::extract<std::string>("MqttChannel", rec)) {
                strm << "CHANNEL[" << std::setw(5) << std::left << v.get() << "] ";
            }
            // Shorten file path example
            if (auto v = boost::log::extract<std::string>("MqttFile", rec)) {
                strm << boost::filesystem::path(v.get()).filename().string() << ":";
            }
            if (auto v = boost::log::extract<unsigned int>("MqttLine", rec)) {
                strm << v.get() << " ";
            }
            if (auto v = boost::log::extract<void const*>("MqttAddress", rec)) {
                strm << "ADDR[" << v.get() << "] ";
            }
#if 0
            if (auto v = boost::log::extract<std::string>("MqttFunction", rec)) {
                strm << v << ":";
            }
#endif
            strm << rec[boost::log::expressions::smessage];
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
                auto channel = boost::log::extract<std::string>("MqttChannel", avs);
                auto severity = boost::log::extract<MQTT_NS::severity_level>("MqttSeverity", avs);
                if (channel) {
                    if (severity) {
                        auto it = threshold.find(channel.get());
                        if (it == threshold.end()) return false;
                        return severity.get() >= it->second;
                    }
                    else {
                        std::cout << "no sev" << std::endl;
                    }
                }
                else {
                        std::cout << "no channel" << std::endl;
                }
            }
            return true;
        };
    sink->set_filter(fil);

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
            { "api", threshold },
            { "cb", threshold },
        }
    );
}

} // namespace MQTT_NS

#endif // MQTT_LOG_HPP
