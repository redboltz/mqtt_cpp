// Copyright Takatoshi Kondo 2021
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(MQTT_BROKER_SECURITY_HPP)
#define MQTT_BROKER_SECURITY_HPP

#include <string>

#include <mqtt/broker/broker_namespace.hpp>
#include <mqtt/broker/subscription_map.hpp>
#include <mqtt/optional.hpp>

#include <map>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/optional.hpp>

MQTT_BROKER_NS_BEGIN

struct security
{

    struct authentication
    {
        enum class method {
            password,
            client_cert,
            anonymous
        };

        method method_;
        MQTT_NS::optional<std::string> password;

        authentication(method method_ = method::password, MQTT_NS::optional<std::string> const &password = MQTT_NS::optional<std::string>())
            : method_(method_), password(password)
        { }

        std::vector<std::string> groups;
    };


    struct authorization
    {
        std::string topic;
        enum class type {
            allow, deny
        };

        type type_;
        std::vector<std::string> sub;
        std::vector<std::string> pub;

        authorization(std::string const& topic, type const& type_)
            : topic(topic), type_(type_)
        { }
    };

    struct group
    {
        std::string name;
        std::vector<std::string> members;
    };

    std::map<std::string, authentication> authentication_;
    std::map<std::string, group> groups_;
    std::map<std::string, authorization> authorization_;
    MQTT_NS::optional<std::string> anonymous;

    MQTT_NS::broker::multiple_subscription_map<std::string, authorization::type> auth_pub_map;

    MQTT_NS::optional<std::string> login_anonymous() {
        return anonymous;
    }

    MQTT_NS::optional<std::string> login(std::string const& username, std::string const& password) const {
        MQTT_NS::optional<std::string> empty_result;
        auto i = authentication_.find(username);
        if (i == authentication_.end() || i->second.method_ != security::authentication::method::password)
            return empty_result;
        return i->second.password == password ? username : empty_result;
    }

    static authorization::type get_auth_type(std::string const& type) {
        if(type == "allow") return authorization::type::allow;
        if(type == "deny") return authorization::type::deny;
        throw std::runtime_error("An invalid authorization type was specified: " + type);
    }

    static bool is_valid_group_name(std::string const& name) {
        return !name.empty() && name[0] == '@'; // TODO: validate utf-8
    }

    static bool is_valid_user_name(std::string const& name) {
        return !name.empty() && name[0] != '@'; // TODO: validate utf-8
    }

    static void validate_entry(security const& security, std::string const& context, std::string const& name) {
        if(is_valid_group_name(name) && security.groups_.find(name) == security.groups_.end())
            throw std::runtime_error("An invalid group name was specified for " + context + ": " + name);
        if(is_valid_user_name(name) && security.authentication_.find(name) == security.authentication_.end())
            throw std::runtime_error("An invalid username name was specified for " + context + ": " + name);
    }

    static void validate(security &security)
    {
        for(auto const& i: security.groups_) {
            for(auto const& j: i.second.members) {           
                auto iter = security.authentication_.find(j);
                if(is_valid_user_name(j) && iter == security.authentication_.end())
                    throw std::runtime_error("An invalid username name was specified for group " + i.first + ": " + j);
            }
        }

        for(auto const &i: security.authorization_) {
            for(auto const& j: i.second.sub) {
                validate_entry(security, "topic " + i.first, j);
            }
            for(auto const& j: i.second.pub) {
                validate_entry(security, "topic " + i.first, j);

                if(is_valid_user_name(j)) {
                    security.auth_pub_map.insert_or_assign(i.first, j, i.second.type_);
                }
                else if(is_valid_group_name(j)) {
                    for(auto const& z: security.groups_[j].members)
                        security.auth_pub_map.insert_or_assign(i.first, z, i.second.type_);
                }
            }
        }
    }

    static void load(std::istream &input, security &security) {
        // Create a root
        boost::property_tree::ptree root;
        boost::property_tree::read_json(input, root);

        for(auto const &i: root.get_child("authentication")) {
            std::string name = i.second.get<std::string>("name");
            if(!is_valid_user_name(name)) throw std::runtime_error("An invalid username was specified: " + name);

            std::string method = i.second.get<std::string>("method");

            if(method == "password") {
                std::string password = i.second.get<std::string>("password");
                authentication auth(authentication::method::password, password);
                security.authentication_.insert( { name, auth });
            } else if(method == "client_cert") {
                authentication auth(authentication::method::client_cert);
                security.authentication_.insert({ name, auth });
            } else if(method == "anonymous") {
                if(security.anonymous) throw std::runtime_error("Only a single anonymous user can be configured, anonymous user: " + *security.anonymous);
                security.anonymous = name;

                authentication auth(authentication::method::anonymous);
                security.authentication_.insert( { name, auth });
            } else throw std::runtime_error("An invalid method was specified: " + method);
        }
        for(auto const& i: root.get_child("group")) {
            std::string name = i.second.get<std::string>("name");
            if(!is_valid_group_name(name)) throw std::runtime_error("An invalid group name was specified: " + name);

            group group;
            if(i.second.get_child_optional("members")) {
                for(auto const& j: i.second.get_child("members")) {
                    auto username = j.second.get_value<std::string>();
                    if(!is_valid_user_name(username)) throw std::runtime_error("An invalid user name was specified: " + username);
                    group.members.push_back(username);
                }
            }

            security.groups_.insert({ name, group });
        }

        for(auto const& i: root.get_child("authorization")) {
            std::string name = i.second.get<std::string>("topic");
            //if(!is_valid_topic(name)) throw std::runtime_error("An invalid topic was specified: " + name);

            auto type = get_auth_type(i.second.get<std::string>("type"));

            authorization auth(name, type);
            if(i.second.get_child_optional("sub")) {
                for(auto const& j: i.second.get_child("sub"))
                    auth.sub.push_back(j.second.get_value<std::string>());
            }

            if(i.second.get_child_optional("pub")) {
                for(auto const& j: i.second.get_child("pub"))
                    auth.pub.push_back(j.second.get_value<std::string>());
            }

            security.authorization_.insert({ name, auth });
        }

        validate(security);
    }

    authorization::type auth_pub(std::string const& topic, std::string const& username) {
        authorization::type result_type = authorization::type::deny;

        auth_pub_map.find(topic, [&](std::string const &allowed_username, authorization::type type) {
            if (allowed_username == username) result_type = type;
        });

        return result_type;
    }
};

MQTT_BROKER_NS_END

#endif // MQTT_BROKER_SECURITY_HPP
