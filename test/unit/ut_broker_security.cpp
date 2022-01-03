// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../common/test_main.hpp"
#include "../common/global_fixture.hpp"

#include <mqtt/broker/security.hpp>

BOOST_AUTO_TEST_SUITE(ut_broker_security)

void load_config(MQTT_NS::broker::security &security, std::string const& value)
{
    std::stringstream input(value);
    security.load_json(input);
}

BOOST_AUTO_TEST_CASE(default_config) {
    MQTT_NS::broker::security security;
    security.default_config();

    BOOST_CHECK(security.authentication_["anonymous"].method_ == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].password);

    BOOST_CHECK(security.login_anonymous());

    BOOST_CHECK(security.auth_pub("topic", "anonymous") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("sub/topic", "anonymous") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("sub/topic1", "anonymous") == MQTT_NS::broker::security::authorization::type::allow);

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("topic"), "anonymous") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("sub/topic"), "anonymous") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("sub/topic1"), "anonymous") == MQTT_NS::broker::security::authorization::type::allow);
}

BOOST_AUTO_TEST_CASE(json_load) {
    MQTT_NS::broker::security security;

    std::string value = "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"75c111ce6542425228c157b1187076ed86e837f6085e3bb30b976114f70abc40\"},{\"name\":\"u2\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\",\"anonymous\"]}],\"authorization\":[{\"topic\":\"#\",\"type\":\"allow\",\"pub\":[\"@g1\"]},{\"topic\":\"#\",\"type\":\"deny\",\"sub\":[\"@g1\"]},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"],\"pub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"],\"pub\":[\"u1\",\"anonymous\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}";

    BOOST_CHECK_NO_THROW(load_config(security, value));

    BOOST_CHECK(security.authentication_.size() == 3);

    BOOST_CHECK(security.authentication_["u1"].method_ == MQTT_NS::broker::security::authentication::method::password);
    BOOST_CHECK(security.authentication_["u1"].password);

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(boost::iequals(*security.authentication_["u1"].password, MQTT_NS::broker::security::hash("aes256:salt:mypassword")));
#endif

    BOOST_CHECK(security.authentication_["u2"].method_ == MQTT_NS::broker::security::authentication::method::client_cert);
    BOOST_CHECK(!security.authentication_["u2"].password);

    BOOST_CHECK(security.authentication_["anonymous"].method_ == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].password);

    BOOST_CHECK(security.groups_.size() == 1);
    BOOST_CHECK(security.groups_["@g1"].members.size() == 3);

    BOOST_CHECK(security.anonymous);
    BOOST_CHECK(*security.anonymous == "anonymous");

    BOOST_CHECK(security.login_anonymous());

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(security.login("u1", "mypassword"));
    BOOST_CHECK(!security.login("u1", "invalidpassword"));
    BOOST_CHECK(!security.login("u3", "invalidpassword"));
#endif

}

BOOST_AUTO_TEST_CASE(check_errors) {
    MQTT_NS::broker::security security;

    BOOST_CHECK(MQTT_NS::broker::security::is_valid_group_name("@test"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_valid_group_name("test"));

    BOOST_CHECK(MQTT_NS::broker::security::is_valid_user_name("test"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_valid_user_name("@test"));

    BOOST_CHECK(MQTT_NS::broker::security::get_auth_type("allow") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(MQTT_NS::broker::security::get_auth_type("deny") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK_THROW(MQTT_NS::broker::security::get_auth_type("invalid"), std::exception);

    // Group references non-existing user
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Auth references non-existing user
    BOOST_CHECK_THROW(load_config(security, "{\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Duplicate user
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"mypassword\"},{\"name\":\"u1\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Duplicate anonymous
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"u1\",\"method\":\"anonymous\",\"password\":\"mypassword\"},{\"name\":\"u1\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Duplicate group
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]},{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Non-existing group
    BOOST_CHECK_THROW(load_config(security, "{\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@nonexist\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}"), std::exception);

    // Invalid username
    BOOST_CHECK_THROW(load_config(security, "{\"authentication\":[{\"name\":\"@u1\",\"method\":\"anonymous\"}]}"), std::exception);

    // Invalid group name
    BOOST_CHECK_THROW(load_config(security, "{\"group\":[{\"name\":\"g1\",\"members\":[\"u1\",\"u2\"]}]}}"), std::exception);

}

BOOST_AUTO_TEST_CASE(check_publish) {
    MQTT_NS::broker::security security;

    std::string value = "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"mypassword\"},{\"name\":\"u2\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\"]}],\"authorization\":[{\"topic\":\"#\",\"type\":\"deny\"},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"],\"pub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"],\"pub\":[\"u1\",\"anonymous\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}";
    BOOST_CHECK_NO_THROW(load_config(security, value));

    BOOST_CHECK(security.auth_pub("topic", "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_pub("sub/topic", "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("sub/topic1", "u1") == MQTT_NS::broker::security::authorization::type::deny);

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("topic"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("sub/topic"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("sub/topic1"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
}

BOOST_AUTO_TEST_CASE(test_hash) {

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(MQTT_NS::broker::security::hash("a quick brown fox jumps over the lazy dog") == "8F1AD6DFFF1A460EB4AB78A5A7C3576209628EA200C1DBC70BDA69938B401309");
#endif

}

std::vector<std::string> get_topic_filter_tokens(MQTT_NS::string_view topic_filter) {
    std::vector<std::string> result;
    MQTT_NS::broker::topic_filter_tokenizer(topic_filter, [&result](auto str) {
        result.push_back(std::string(str));
        return true;
    });

    return result;
}

static constexpr char topic_filter_separator = '/';
bool is_hash(std::string const &level) { return level == "#"; }
bool is_plus(std::string const &level) { return level == "+"; }
bool is_literal(std::string const &level) { return !is_hash(level) && !is_plus(level); }

MQTT_NS::optional<std::string> is_subscribe_allowed(std::vector<std::string> const &authorized_filter, std::vector<std::string> const &subscription_filter)
{
    MQTT_NS::optional<std::string> result;
    auto append_result = [&result](std::string const &token) {
          if (result) {
              result.value() += topic_filter_separator + token;
          } else {
              result = MQTT_NS::optional<std::string>(token);
          }
      };

    auto filter_begin = authorized_filter.begin();
    auto subscription_begin = subscription_filter.begin();

    while (filter_begin < authorized_filter.end() && subscription_begin < subscription_filter.end()) {
        auto auth = *filter_begin;
        ++filter_begin;

        auto sub = *subscription_begin;
        ++subscription_begin;

        if (is_hash(auth)) {
            append_result(sub);

            while (subscription_begin < subscription_filter.end()) {
                append_result(*subscription_begin);
                ++subscription_begin;
            }

            return result;
        }

       if (is_hash(sub)) {
            append_result(auth);

            while(filter_begin < authorized_filter.end()) {
                append_result(*filter_begin);
                ++filter_begin;
            }

            return result;
        }

        if (is_plus(auth)) {
            append_result(sub);
        }  else if (is_plus(sub)) {
            append_result(auth);
        } else
        {
            if (auth != sub)  {
                return MQTT_NS::optional<std::string>();
            }

            append_result(auth);
        }
    }

    if ( filter_begin < authorized_filter.end() || subscription_begin < subscription_filter.end()) {
        return MQTT_NS::optional<std::string>();
    }

    return result;
}

MQTT_NS::optional<std::string> is_subscribe_allowed(std::string const &authorized_filter, std::string const &subscription_filter)
{
    return is_subscribe_allowed(get_topic_filter_tokens(authorized_filter), get_topic_filter_tokens(subscription_filter));
}

bool is_subscribe_denied(std::vector<std::string> const &deny_filter, std::vector<std::string> const &subscription_filter)
{
    auto filter_begin = deny_filter.begin();
    auto subscription_begin = subscription_filter.begin();

    while (filter_begin < deny_filter.end() && subscription_begin < subscription_filter.end()) {
        std::string deny = *filter_begin;
        ++filter_begin;

        std::string sub = *subscription_begin;
        ++subscription_begin;

        if(deny != sub) {
            if (is_hash(deny)) {
                return true;
            }

            if (is_hash(sub)) {
                return false;
            }

            if (is_plus(deny)) {
                return true;
            }

            return false;
        }
    }

    return (filter_begin == deny_filter.end() && subscription_begin == subscription_filter.end());
}

bool is_subscribe_denied(std::string const &deny_filter, std::string const &subscription_filter)
{
    return is_subscribe_denied(get_topic_filter_tokens(deny_filter), get_topic_filter_tokens(subscription_filter));
}

std::vector<std::string> get_auth_topics(MQTT_NS::broker::security const &security, std::string const &username, std::string const &topic)
{
    auto result = security.get_auth_sub_by_user(username);

    std::vector<std::string> auth_topics;
    for(auto const &i: result) {
        if (i.second == MQTT_NS::broker::security::authorization::type::allow) {
            auto entry = is_subscribe_allowed(i.first, topic);
            if (entry) auth_topics.push_back(entry.value());
        } else {
            std::vector<std::string> new_auth_topics;
            for(auto const &j: auth_topics) {
                if(!is_subscribe_denied(i.first, topic))
                    new_auth_topics.push_back(j);
            }
            auth_topics = new_auth_topics;
        }
    }

    return auth_topics;
}

BOOST_AUTO_TEST_CASE(authorized_check) {
    BOOST_CHECK(is_subscribe_allowed("example/value/a", "example/value/a").value() == "example/value/a");
    BOOST_CHECK(is_subscribe_allowed("example/+/a", "example/value/a").value() == "example/value/a");
    BOOST_CHECK(is_subscribe_allowed("example/value/a", "example/+/a").value() == "example/value/a");
    BOOST_CHECK(is_subscribe_allowed("example/#", "example/value/a").value() == "example/value/a");
    BOOST_CHECK(is_subscribe_allowed("example/value/a", "example/#").value() == "example/value/a");
    BOOST_CHECK(!is_subscribe_allowed("example/deny", "example/test"));
}

BOOST_AUTO_TEST_CASE(deny_check) {
    BOOST_CHECK(is_subscribe_denied("example/value/a", "example/value/a"));
    BOOST_CHECK(!is_subscribe_denied("example/value/a", "example/value/b"));

    BOOST_CHECK(is_subscribe_denied("example/+/a", "example/value/a"));
    BOOST_CHECK(is_subscribe_denied("example/+/a", "example/+/a"));
    BOOST_CHECK(!is_subscribe_denied("example/value/a", "example/+/a"));

    BOOST_CHECK(is_subscribe_denied("example/#", "example/#"));
    BOOST_CHECK(is_subscribe_denied("example/#", "example/+"));
    BOOST_CHECK(!is_subscribe_denied("example/+", "example/#"));
    BOOST_CHECK(is_subscribe_denied("example/#", "example/value"));
    BOOST_CHECK(!is_subscribe_denied("example/value", "example/#"));
}

BOOST_AUTO_TEST_CASE(auth_check) {
    MQTT_NS::broker::security security;
    std::string value = "{\"authentication\":[{\"name\":\"u1\",\"method\":\"password\",\"password\":\"75c111ce6542425228c157b1187076ed86e837f6085e3bb30b976114f70abc40\"},{\"name\":\"u2\",\"method\":\"client_cert\",\"field\":\"CNAME\"},{\"name\":\"anonymous\",\"method\":\"anonymous\"}],\"group\":[{\"name\":\"@g1\",\"members\":[\"u1\",\"u2\",\"anonymous\"]}],\"authorization\":[{\"topic\":\"#\",\"type\":\"allow\",\"pub\":[\"@g1\"]},{\"topic\":\"#\",\"type\":\"deny\",\"sub\":[\"@g1\"]},{\"topic\":\"sub/#\",\"type\":\"allow\",\"sub\":[\"@g1\"],\"pub\":[\"@g1\"]},{\"topic\":\"sub/topic1\",\"type\":\"deny\",\"sub\":[\"u1\",\"anonymous\"],\"pub\":[\"u1\",\"anonymous\"]}],\"config\":{\"hash\":\"aes256\",\"salt\":\"salt\"}}";
    BOOST_CHECK_NO_THROW(load_config(security, value));

    auto result = security.get_auth_sub_by_user("u1");
    BOOST_CHECK(result.size() == 3);

    BOOST_CHECK(!get_auth_topics(security, "u1", "sub/test").empty());
    BOOST_CHECK(get_auth_topics(security, "u1", "sub/topic1").empty());
    BOOST_CHECK(get_auth_topics(security, "u1", "example/topic1").empty());
}

BOOST_AUTO_TEST_SUITE_END()
