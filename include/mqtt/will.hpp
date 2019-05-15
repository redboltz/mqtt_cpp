// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_WILL_HPP)
#define MQTT_WILL_HPP

#include <string>

#include <mqtt/qos.hpp>
#include <mqtt/property_variant.hpp>

namespace mqtt {

class will {
public:
    /**
     * @brief constructor
     * @param topic
     *        A topic name to publish as a will
     * @param message
     *        The contents to publish as a will
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718038<BR>
     *        3.3.1.3 RETAIN
     * @param qos
     *        mqtt::qos
     */
    will(std::string topic,
         std::string message,
         bool retain,
         std::uint8_t qos,
         std::vector<v5::property_variant> props = {})
        :topic_(std::move(topic)),
         message_(std::move(message)),
         retain_(retain),
         qos_(qos),
         props_(std::move(props))
    {}

    /**
     * @brief constructor (QoS0)
     * @param topic
     *        A topic name to publish as a will
     * @param message
     *        The contents to publish as a will
     * @param retain
     *        A retain flag. If set it to true, the contents is retained.<BR>
     *        See http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/os/mqtt-v3.1.1-os.html#_Toc398718038<BR>
     *        3.3.1.3 RETAIN
     */
    will(std::string topic,
         std::string message,
         bool retain = false,
         std::vector<v5::property_variant> props = {})
        :will(std::move(topic), std::move(message), retain, qos::at_most_once, std::move(props))
    {}

    /**
     * @brief constructor (retain = false)
     * @param topic
     *        A topic name to publish as a will
     * @param message
     *        The contents to publish as a will
     * @param qos
     *        mqtt::qos
     */
    will(std::string topic,
         std::string message,
         std::uint8_t qos,
         std::vector<v5::property_variant> props = {})
        :will(std::move(topic), std::move(message), false, qos, std::move(props))
    {}
    std::string const& topic() const {
        return topic_;
    }
    std::string& topic() {
        return topic_;
    }
    std::string const& message() const {
        return message_;
    }
    std::string& message() {
        return message_;
    }
    bool retain() const {
        return retain_;
    }
    std::uint8_t qos() const {
        return qos_;
    }
    std::vector<v5::property_variant> const& props() const {
        return props_;
    }
    std::vector<v5::property_variant>& props() {
        return props_;
    }

private:
    std::string topic_;
    std::string message_;
    bool retain_ = false;
    std::uint8_t qos_ = 0;
    std::vector<v5::property_variant> props_;
};

} // namespace mqtt

#endif // MQTT_WILL_HPP
