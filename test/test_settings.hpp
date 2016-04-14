// Copyright Takatoshi Kondo 2015
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_TEST_SETTINGS_HPP)
#define MQTT_TEST_SETTINGS_HPP

#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <mqtt/client.hpp>

constexpr char const* broker_url = "test.mosquitto.org";
constexpr uint16_t const broker_notls_port = 1883;
constexpr uint16_t const broker_tls_port = 8883;

class uuid : public boost::uuids::uuid {
public:
    uuid()
        : boost::uuids::uuid(boost::uuids::random_generator()())
    {}

    explicit uuid(boost::uuids::uuid const& u)
        : boost::uuids::uuid(u)
    {}
};

inline std::string const& topic_base() {
    static std::string test_topic = boost::uuids::to_string(uuid());
    return test_topic;
}

inline std::string const& cid1() {
    static std::string test_cid = boost::uuids::to_string(uuid());
    return test_cid;
}

inline std::string const& cid2() {
    static std::string test_cid = boost::uuids::to_string(uuid());
    return test_cid;
}

struct fixture_clear_retain {
    fixture_clear_retain() {
        boost::asio::io_service ios;
        std::uint16_t pid_clear1;
        std::uint16_t pid_clear2;
        std::uint16_t pid_sub1;
        std::uint16_t pid_sub2;
        bool clear1 = false;
        bool clear2 = false;
        bool sub1 = false;
        bool sub2 = false;
        std::size_t count = 0;
        auto c = mqtt::make_client(ios, broker_url, broker_notls_port);
        c.set_clean_session(true);
        c.set_connack_handler(
            [&]
            (bool, std::uint8_t connack_return_code) {
                BOOST_TEST(connack_return_code == mqtt::connect_return_code::accepted);
                // Clear retaind contents
                pid_sub1 = c.subscribe(topic_base() + "/topic1", mqtt::qos::at_least_once);
                pid_sub2 = c.subscribe(topic_base() + "/topic2", mqtt::qos::at_least_once);
            });
        c.set_suback_handler(
            [&]
            (std::uint16_t pid, std::vector<boost::optional<std::uint8_t>>) {
                if (pid == pid_sub1) sub1 = true;
                if (pid == pid_sub2) sub2 = true;
                if (sub1 && sub2) {
                    pid_clear1 = c.publish_at_least_once(topic_base() + "/topic1", "", true);
                    pid_clear2 = c.publish_at_least_once(topic_base() + "/topic2", "", true);
                }
            });
        c.set_puback_handler(
            [&]
            (std::uint16_t pid) {
                if (pid == pid_clear1) clear1 = true;
                if (pid == pid_clear2) clear2 = true;
                if (count == 2 && clear1 && clear2) {
                    c.disconnect();
                }
            });
        c.set_publish_handler(
            [&]
            (std::uint8_t,
             boost::optional<std::uint16_t>,
             std::string,
             std::string) {
                ++count;
                if (count == 2 && clear1 && clear2) {
                    c.disconnect();
                }
            });
        c.connect();
        ios.run();
    }
};

#endif // MQTT_TEST_SETTINGS_HPP
