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

std::string json_remove_comments(std::string const& value) {
    std::stringstream value_input(value);
    return MQTT_NS::broker::json_remove_comments(value_input);
}

BOOST_AUTO_TEST_CASE(json_comments) {
    BOOST_CHECK(json_remove_comments("test") == "test");
    BOOST_CHECK(json_remove_comments("#test\ntest") == "\ntest");
    BOOST_CHECK(json_remove_comments("'#test'") == "'#test'");
    BOOST_CHECK(json_remove_comments("\"#test\"") == "\"#test\"");
    BOOST_CHECK(json_remove_comments("\"'#test'\"") == "\"'#test'\"");
    BOOST_CHECK(json_remove_comments("'\"#test\"'") == "'\"#test\"'");
    BOOST_CHECK(json_remove_comments("") == "");
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
        { # JSON Comment
            "authentication": [{
                "name": "u1",
                "method": "sha256",
                "salt": "salt",
                "digest": "38ea2e5e88fcd692fe177c6cada15e9b2db6e70bee0a0d6678c8d3b2a9aae2ad"
            }, {
                "name": "u2",
                "method": "client_cert"
            }, {
                "name": "u3",
                "method": "plain_password",
                "password": "mypassword"
            }, {
                "name": "anonymous",
                "method": "anonymous"
            }],
            "groups": [{
                "name": "@g1",
                "members": ["u1", "u2", "anonymous"]
            }],
            "authorization": [{
                "topic": "#",
                "allow": { "pub": ["@g1"] }
            }, {
                "topic": "#",
                "deny": { "sub": ["@g1"] }
            }, {
                "topic": "sub/#",
                "allow": {
                    "sub": ["@g1"],
                    "pub": ["@g1"]
                }
            }, {
                "topic": "sub/topic1",
                "deny": {
                    "sub": ["u1", "anonymous"],
                    "pub": ["u1", "anonymous"]
                }
            }]
        }
        )*";

    BOOST_CHECK_NO_THROW(load_config(security, value));

    BOOST_CHECK(security.authentication_.size() == 4);

    BOOST_CHECK(security.authentication_["u1"].auth_method == MQTT_NS::broker::security::authentication::method::sha256);
    BOOST_CHECK(security.authentication_["u1"].digest.value() == "38ea2e5e88fcd692fe177c6cada15e9b2db6e70bee0a0d6678c8d3b2a9aae2ad");
    BOOST_CHECK(security.authentication_["u1"].salt == "salt");

#if defined(MQTT_USE_TLS)
    BOOST_CHECK(boost::iequals(*security.authentication_["u1"].digest, MQTT_NS::broker::security::sha256hash("saltmypassword")));
#endif

    BOOST_CHECK(security.authentication_["u2"].auth_method == MQTT_NS::broker::security::authentication::method::client_cert);
    BOOST_CHECK(!security.authentication_["u2"].digest);
    BOOST_CHECK(security.authentication_["u2"].salt.empty());

    BOOST_CHECK(security.authentication_["u3"].auth_method == MQTT_NS::broker::security::authentication::method::plain_password);
    BOOST_CHECK(security.authentication_["u3"].digest.value() == "mypassword");
    BOOST_CHECK(security.authentication_["u3"].salt.empty());

    BOOST_CHECK(security.authentication_["anonymous"].auth_method == MQTT_NS::broker::security::authentication::method::anonymous);
    BOOST_CHECK(!security.authentication_["anonymous"].digest);
    BOOST_CHECK(security.authentication_["anonymous"].salt.empty());

    BOOST_CHECK(security.groups_.size() == 2);
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
            {  # JSON Comment
                "groups": [{
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
                    "allow": {
                        "sub": ["@g1"]
                    }
                }, {
                    "topic": "sub/topic1",
                    "deny": {
                        "sub": ["u1", "anonymous"]
                    }
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
                "groups": [{
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }, {
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, duplicate_group), std::exception);

    // Redefine any group
    std::string redefine_any_group = R"*(
            {
                "groups": [{
                    "name": "@any",
                    "members": ["u1", "u2"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, redefine_any_group), std::exception);

    // Non-existing group
    std::string non_existing_group = R"*(
            {
                "authorization": [{
                    "topic": "#",
                    "type": "deny"
                }, {
                    "topic": "sub/#",
                    "allow": { "sub": ["@nonexist"] }
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
                "groups": [{
                    "name": "g1",
                    "members": ["u1", "u2"]
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, invalid_group_name), std::exception);

    // Invalid field
    std::string invalid_field = R"*(
            {
                "authentication": [{
                    "name": "u1",
                    "method": "client_cert",
                    "field": "other",
                }]
            }
        )*";
    BOOST_CHECK_THROW(load_config(security, invalid_field), std::exception);
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
                    "method": "client_cert"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }],
                "groups": [{
                    "name": "@g1",
                    "members": ["u1", "u2"]
                }],
                "authorization": [{
                    "topic": "#",
                    "deny": {
                        "sub": ["@g1"],
                        "pub": ["@g1"]
                    }
                }, {
                    "topic": "sub/#",
                    "allow": {
                        "sub": ["@g1"],
                        "pub": ["@g1"]
                    }
                }, {
                    "topic": "sub/topic1",
                    "deny": {
                        "sub": ["u1", "anonymous"],
                        "pub": ["u1", "anonymous"]
                    }
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

BOOST_AUTO_TEST_CASE(check_publish_any) {
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
                    "method": "client_cert"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }],
                "authorization": [{
                    "topic": "#",
                    "deny": {
                        "sub": ["@any"],
                        "pub": ["@any"]
                    }
                }, {
                    "topic": "sub/#",
                    "allow": {
                        "sub": ["@any"],
                        "pub": ["@any"]
                    }
                }, {
                    "topic": "sub/topic1",
                    "deny": {
                        "sub": ["u1", "anonymous"],
                        "pub": ["u1", "anonymous"]
                    }
                }]
            }
        )*";
    try {
        load_config(security, value);
    } catch(std::exception &e) {
        MQTT_LOG("mqtt_test", error)
            << "exception:" << e.what();
    }

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
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/+/b"), "example/value/a"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/+/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/#"), "example/value/a").value() == "example/value/a");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/#").value() == "example/value/a");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/deny"), "example/test"));

    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+"), "t1").value() == "t1");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+"), "t1/"));

    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+"), "t1/t2"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+"), "t1/t2/t3"));

    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/+"), "t1"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/+"), "t1/").value() == "t1/");
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/+"), "t1/t2").value() == "t1/t2");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/+"), "t1/t2/t3"));

    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/"), "t1"));
    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/"), "t1/").value() == "t1/");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_allowed(
        MQTT_NS::broker::security::get_topic_filter_tokens("+/"), "t1/t2"));

}

BOOST_AUTO_TEST_CASE(deny_check) {
    BOOST_CHECK(MQTT_NS::broker::topic_filter_tokenizer("example/value/a", [](auto) { return true; } ) == 3);

    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/value/a"));
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(
        MQTT_NS::broker::security::get_topic_filter_tokens("example/value/a"), "example/value/b"));

    BOOST_CHECK(MQTT_NS::broker::topic_filter_tokenizer("example/+/a", [](auto) { return true; } ) == 3);
    BOOST_CHECK(MQTT_NS::broker::topic_filter_tokenizer("example/value/a", [](auto) { return true; } ) == 3);

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
            # JSON Comment
            {
                "authentication": [{
                    "name": "u1",
                    "method": "sha256",
                    "salt": "salt",
                    "digest": "75c111ce6542425228c157b1187076ed86e837f6085e3bb30b976114f70abc40"
                }, {
                    "name": "u2",
                    "method": "client_cert"
                }, {
                    "name": "anonymous",
                    "method": "anonymous"
                }],
                "groups": [{
                    "name": "@g1",
                    "members": ["u1", "u2", "anonymous"]
                }],
                "authorization": [{
                    "topic": "#",
                    "allow": { "pub": ["@g1"] }
                }, {
                    "topic": "#",
                    "deny": { "sub": ["@g1"] }
                }, {
                    "topic": "sub/#",
                    "allow": {
                        "sub": ["@g1"],
                        "pub": ["@g1"]
                    }
                }, {
                    "topic": "sub/topic1",
                    "deny": {
                        "sub": ["u1", "anonymous"],
                        "pub": ["u1", "anonymous"]
                    }
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
            # JSON Comment
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
                        "deny": { "sub": ["u1","u2"] }
                    }
                    ,
                    {
                        "topic": "#",
                        "allow": {
                            "sub": ["u1"]
                        }
                    }
                ]
            }
        )*";

    BOOST_CHECK_NO_THROW(load_config(security, test_2));

    // u1 is allowed to subscribe, u2 is not
    BOOST_CHECK(!security.get_auth_sub_topics("u1", "sub/test").empty());
    BOOST_CHECK(security.get_auth_sub_topics("u2", "sub/test").empty());
}

BOOST_AUTO_TEST_CASE(auth_check_dynamic) {
    MQTT_NS::broker::security security;
    std::string test = R"*(
            # JSON Comment
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    },
                    {
                        "name": "u2",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                     {
                        "topic": "t1",
                        "allow": { "pub":["u1"], "sub":["u1"] }
                    }
                ]
            }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security, test));

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t1"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t1"), "u2") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_pub("t1", "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("t1", "u2") == MQTT_NS::broker::security::authorization::type::deny);

    auto rule_nr = security.add_auth("t1",
        { "@any" }, MQTT_NS::broker::security::authorization::type::allow,
        { "u2" }, MQTT_NS::broker::security::authorization::type::allow);

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t1"), "u2") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("t1", "u2") == MQTT_NS::broker::security::authorization::type::allow);

    security.remove_auth(rule_nr);

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t1"), "u2") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_pub("t1", "u2") == MQTT_NS::broker::security::authorization::type::deny);

}

BOOST_AUTO_TEST_CASE(auth_check_plus) {
    BOOST_CHECK(MQTT_NS::broker::security::get_topic_filter_tokens("+/").size() == 2);
    BOOST_CHECK(MQTT_NS::broker::security::get_topic_filter_tokens("+/+/").size() == 3);

    MQTT_NS::broker::security security_1;
    std::string test_1 = R"*(
            # JSON Comment
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                                         {
                                "topic": "+",
                                "allow": { "pub":["u1"], "sub":["u1"] }
                                }
                ]
            }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security_1, test_1));

    BOOST_CHECK(security_1.auth_sub_user(security_1.auth_sub("t1"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security_1.auth_sub_user(security_1.auth_sub("t1/"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security_1.auth_sub_user(security_1.auth_sub("t1/t2"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security_1.auth_sub_user(security_1.auth_sub("t1/t2/t3"), "u1") == MQTT_NS::broker::security::authorization::type::deny);

    MQTT_NS::broker::security security_2;
    std::string test_2 = R"*(
            # JSON Comment
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                                        {
                                "topic": "+/+",
                                "allow": { "pub":["u1"], "sub":["u1"] }
                                }
                ]
            }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security_2, test_2));

    BOOST_CHECK(security_2.auth_sub_user(security_2.auth_sub("t1"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security_2.auth_sub_user(security_2.auth_sub("t1/t2"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security_2.auth_sub_user(security_2.auth_sub("t1/t2/"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security_2.auth_sub_user(security_2.auth_sub("t1/t2/t3"), "u1") == MQTT_NS::broker::security::authorization::type::deny);

    MQTT_NS::broker::security security_3;
    std::string test_3 = R"*(
            # JSON Comment
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                                        {
                                "topic": "+/",
                                "allow": { "pub":["u1"], "sub":["u1"] }
                                }
                ]
            }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security_3, test_3));

    BOOST_CHECK(security_3.auth_sub_user(security_3.auth_sub("t1"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security_3.auth_sub_user(security_3.auth_sub("t1/"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security_3.auth_sub_user(security_3.auth_sub("t1/t2"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
}

BOOST_AUTO_TEST_CASE(priority_test) {
    MQTT_NS::broker::security security;
    std::string test = R"*(
            # JSON Comment
            {
                "authentication": [
                    {
                        "name": "u1",
                        "method": "plain_password",
                        "password": "hoge"
                    }
                ],
                "authorization": [
                    {
                        "topic": "t1",
                        "allow": { "pub":["u1"], "sub":["u1"] }

                    }
                    ,
                    {
                        "topic": "#",
                        "deny": { "pub":["u1"], "sub":["u1"] }

                    }
                    ,
                    {
                        "topic": "t2",
                        "allow": { "pub":["u1"], "sub":["u1"] }
                    }
                ]
            }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security, test));

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t1"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t2"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("t3"), "u1") == MQTT_NS::broker::security::authorization::type::deny);

    BOOST_CHECK(security.auth_pub("t1", "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_pub("t2", "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_pub("t3", "u1") == MQTT_NS::broker::security::authorization::type::deny);

}

BOOST_AUTO_TEST_CASE(subscription_level_check) {
    MQTT_NS::broker::security security;
    std::string test = R"*(
        {
            # Configure username/login
            "authentication": [
                {
                    "name": "u1",
                    "method": "plain_password",
                    "password": "hoge"
                }
            ],
            # Give access to topics
            "authorization": [
                {
                    "topic": "#",
                    "deny": { "pub":["u1"], "sub":["u1"] }

                }
                ,
                {
                    "topic": "1/#",
                    "allow": { "pub":["u1"], "sub":["u1"] }

                }
                ,
                {
                    "topic": "1/2/#",
                    "deny": { "pub":["u1"], "sub":["u1"] }

                }
            ]
        }
        )*";
    BOOST_CHECK_NO_THROW(load_config(security, test));

    BOOST_CHECK(MQTT_NS::broker::security::is_subscribe_denied(security.get_topic_filter_tokens("#"), "1/2"));
    BOOST_CHECK(*MQTT_NS::broker::security::is_subscribe_allowed(security.get_topic_filter_tokens("1/#"), "1/2") == "1/2");
    BOOST_CHECK(!MQTT_NS::broker::security::is_subscribe_denied(security.get_topic_filter_tokens("1/2/#"), "1/2"));

    BOOST_CHECK(security.auth_sub_user(security.auth_sub("1/2"), "u1") == MQTT_NS::broker::security::authorization::type::allow);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("1/2/3"), "u1") == MQTT_NS::broker::security::authorization::type::deny);
    BOOST_CHECK(security.auth_sub_user(security.auth_sub("1/2/"), "u1") == MQTT_NS::broker::security::authorization::type::deny);

    BOOST_CHECK(security.is_subscribe_authorized("u1", "1/2"));
    BOOST_CHECK(!security.is_subscribe_authorized("u1", "1/2/3"));
    BOOST_CHECK(!security.is_subscribe_authorized("u1", "1/2/"));



}

BOOST_AUTO_TEST_SUITE_END()
