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

    BOOST_CHECK(security.authentication_["anonymous"].auth_method == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].digest);

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

    std::string value = R"*(
        {
            "authentication": [{
                "name": "u1",
                "method": "sha256",
                "salt": "salt",
                "digest": "38ea2e5e88fcd692fe177c6cada15e9b2db6e70bee0a0d6678c8d3b2a9aae2ad"
            }, {
                "name": "u2",
                "method": "client_cert",
                "field": "CNAME"
            }, {
                "name": "u3",
                "method": "plain_password",
                "password": "mypassword"
            }, {
                "name": "anonymous",
                "method": "anonymous"
            }],
            "group": [{
                "name": "@g1",
                "members": ["u1", "u2", "anonymous"]
            }],
            "authorization": [{
                "topic": "#",
                "type": "allow",
                "pub": ["@g1"]
            }, {
                "topic": "#",
                "type": "deny",
                "sub": ["@g1"]
            }, {
                "topic": "sub/#",
                "type": "allow",
                "sub": ["@g1"],
                "pub": ["@g1"]
            }, {
                "topic": "sub/topic1",
                "type": "deny",
                "sub": ["u1", "anonymous"],
                "pub": ["u1", "anonymous"]
            }]
        }
        )*";

    BOOST_CHECK_NO_THROW(load_config(security, value));

    BOOST_CHECK(security.authentication_.size() == 4);

    BOOST_CHECK(security.authentication_["u1"].auth_method == MQTT_NS::broker::security::authentication::method::sha256);
    BOOST_CHECK(security.authentication_["u1"].digest.value() == "38ea2e5e88fcd692fe177c6cada15e9b2db6e70bee0a0d6678c8d3b2a9aae2ad");
    BOOST_CHECK(security.authentication_["u1"].salt.value() == "salt");

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(boost::iequals(*security.authentication_["u1"].digest, MQTT_NS::broker::security::sha256hash("saltmypassword")));
#endif

    BOOST_CHECK(security.authentication_["u2"].auth_method == MQTT_NS::broker::security::authentication::method::client_cert);
    BOOST_CHECK(!security.authentication_["u2"].digest);
    BOOST_CHECK(!security.authentication_["u2"].salt);

    BOOST_CHECK(security.authentication_["u3"].auth_method == MQTT_NS::broker::security::authentication::method::plain_password);
    BOOST_CHECK(security.authentication_["u3"].digest.value() == "mypassword");
    BOOST_CHECK(!security.authentication_["u3"].salt);

    BOOST_CHECK(security.authentication_["anonymous"].auth_method == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].digest);
    BOOST_CHECK(!security.authentication_["anonymous"].salt);

    BOOST_CHECK(security.groups_.size() == 1);
    BOOST_CHECK(security.groups_["@g1"].members.size() == 3);

    BOOST_CHECK(security.anonymous);
    BOOST_CHECK(*security.anonymous == "anonymous");

    BOOST_CHECK(security.login_anonymous());

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(security.login("u1", "mypassword"));
    BOOST_CHECK(!security.login("u1", "invalidpassword"));    
#endif

    BOOST_CHECK(security.login("u3", "mypassword"));
    BOOST_CHECK(!security.login("u3", "invalidpassword"));
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
    std::string nonexisting_1 = R"*(
            {
                "group": [{
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }]
            }
        )*";

    BOOST_CHECK_THROW(load_config(security, nonexisting_1), std::exception);

    // Auth references non-existing user
    std::string nonexisting_2 = R"*(
            {
                "authorization": [{
                    "topic": "#",
                    "type": "deny"
                }, {
                    "topic": "sub/#",
                    "type": "allow",
                    "sub": ["@g1"]
                }, {
                    "topic": "sub/topic1",
                    "type": "deny",
                    "sub": ["u1", "anonymous"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, nonexisting_2), std::exception);

    // Duplicate user
    std::string duplicate_1 = R"*(
            {
                "authentication": [{
                    "name": "u1",
                    "method": "client_cert"
                }, {
                    "name": "u1",
                    "method": "client_cert"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, duplicate_1), std::exception);

    // Duplicate anonymous
    std::string duplicate_anonymous = R"*(
            {
                "authentication": [{
                    "name": "anonymous",
                    "method": "anonymous"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, duplicate_anonymous), std::exception);

    // Duplicate group
    std::string duplicate_group = R"*(
            {
                "group": [{
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }, {
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, duplicate_group), std::exception);

    // Non-existing group
    std::string non_existing_group = R"*(
            {
                "authorization": [{
                    "topic": "#",
                    "type": "deny"
                }, {
                    "topic": "sub/#",
                    "type": "allow",
                    "sub": ["@nonexist"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, non_existing_group), std::exception);

    // Invalid username
    std::string invalid_username = R"*(
            {
                "authentication": [{
                    "name": "@u1",
                    "method": "anonymous"
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, invalid_username), std::exception);

    // Invalid group name
    std::string invalid_group_name = R"*(
            {
                "group": [{
                    "name": "g1",
                    "members": ["u1", "u2"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, invalid_group_name), std::exception);

}

BOOST_AUTO_TEST_CASE(check_publish) {
    MQTT_NS::broker::security security;

    std::string value = R"*(
            {
                "authentication": [{
                    "name": "u1",
                    "method": "sha256",
                    "salt": "salt",
                    "digest": "mypassword"
                }, {
                    "name": "u2",
                    "method": "client_cert",
                    "field": "CNAME"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }],
                "group": [{
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }],
                "authorization": [{
                    "topic": "#",
                    "type": "deny"
                }, {
                    "topic": "sub/#",
                    "type": "allow",
                    "sub": ["@g1"],
                    "pub": ["@g1"]
                }, {
                    "topic": "sub/topic1",
                    "type": "deny",
                    "sub": ["u1", "anonymous"],
                    "pub": ["u1", "anonymous"]
                }]
            }
        )*";
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
    BOOST_CHECK(MQTT_NS::broker::security::sha256hash("a quick brown fox jumps over the lazy dog") == "8F1AD6DFFF1A460EB4AB78A5A7C3576209628EA200C1DBC70BDA69938B401309");
#endif

}

BOOST_AUTO_TEST_CASE(authorized_check) {
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/value/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/+/a"), "example/value/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/+/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/#"), "example/value/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/#").value() == "example/value/a");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/deny"), "example/test"));
}

BOOST_AUTO_TEST_CASE(deny_check) {
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/value/a"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/value/b"));

    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/+/a"), "example/value/a"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/+/a"), "example/+/a"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/+/a"));

    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/#"), "example/#"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/#"), "example/+"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/+"), "example/#"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/#"), "example/value"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value"), "example/#"));
}

BOOST_AUTO_TEST_CASE(auth_check) {
    MQTT_NS::broker::security security;
    std::string test_1 = R"*(
            {
                "authentication": [{
                    "name": "u1",
                    "method": "sha256",
                    "salt": "salt",
                    "digest": "75c111ce6542425228c157b1187076ed86e837f6085e3bb30b976114f70abc40"
                }, {
                    "name": "u2",
                    "method": "client_cert",
                    "field": "CNAME"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }],
                "group": [{
                    "name": "@g1",
                    "members": ["u1", "u2", "anonymous"]
                }],
                "authorization": [{
                    "topic": "#",
                    "type": "allow",
                    "pub": ["@g1"]
                }, {
                    "topic": "#",
                    "type": "deny",
                    "sub": ["@g1"]
                }, {
                    "topic": "sub/#",
                    "type": "allow",
                    "sub": ["@g1"],
                    "pub": ["@g1"]
                }, {
                    "topic": "sub/topic1",
                    "type": "deny",
                    "sub": ["u1", "anonymous"],
                    "pub": ["u1", "anonymous"]
                }]
            }
        )*";

    BOOST_CHECK_NO_THROW(load_config(security, test_1));

    std::size_t count = 0;
    security.get_auth_sub_by_user("u1", [&count](auto const &) { ++count; });
    BOOST_CHECK(count == 3);

    BOOST_CHECK(!security.get_auth_sub_topics("u1", "sub/test").empty());
    BOOST_CHECK(security.get_auth_sub_topics("u1", "sub/topic1").empty());
    BOOST_CHECK(security.get_auth_sub_topics("u1", "example/topic1").empty());

    std::string test_2 = R"*(
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                    ,
                    {
                        "name": "u2",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                    {
                        "topic": "#",
                        "type": "deny",
                        "sub": ["u1","u2"]
                    }
                    ,
                    {
                        "topic": "#",
                        "type": "allow",
                        "sub": ["u1"]
                    }
                ]
            }
        )*";

    BOOST_CHECK_NO_THROW(load_config(security, test_2));

    // u1 is allowed to subscribe, u2 is not
    BOOST_CHECK(!security.get_auth_sub_topics("u1", "sub/test").empty());
    BOOST_CHECK(security.get_auth_sub_topics("u2", "sub/test").empty());
}

BOOST_AUTO_TEST_SUITE_END()
