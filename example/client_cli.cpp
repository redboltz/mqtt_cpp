// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/config.hpp>

#include <iostream>
#include <iomanip>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/format.hpp>

#include <mqtt/setup_log.hpp>
#include <mqtt/sync_client.hpp>
#include <mqtt/unique_scope_guard.hpp>

inline void print_props(std::string prefix, MQTT_NS::v5::properties const& props) {
    for (auto const& p : props) {
        MQTT_NS::visit(
            MQTT_NS::make_lambda_visitor(
                [&](MQTT_NS::v5::property::payload_format_indicator const& t) {
                    std::cout << prefix << "payload_format_indicator: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::message_expiry_interval const& t) {
                    std::cout << prefix << "message_expiry_interval: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::content_type const& t) {
                    std::cout << prefix << "content_type: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::response_topic const& t) {
                    std::cout << prefix << "response_topic: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::correlation_data const& t) {
                    std::cout << prefix << "correlation_data: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::subscription_identifier const& t) {
                    std::cout << prefix << "subscription_identifier: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::session_expiry_interval const& t) {
                    std::cout << prefix << "session_expiry_interval: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::assigned_client_identifier const& t) {
                    std::cout << prefix << "assigned_client_identifier: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::server_keep_alive const& t) {
                    std::cout << prefix << "server_keep_alive: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::authentication_method const& t) {
                    std::cout << prefix << "authentication_method: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::authentication_data const& t) {
                    std::cout << prefix << "authentication_data: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::request_problem_information const& t) {
                    std::cout << prefix << "request_problem_information: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::will_delay_interval const& t) {
                    std::cout << prefix << "will_delay_interval: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::request_response_information const& t) {
                    std::cout << prefix << "request_response_information: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::response_information const& t) {
                    std::cout << prefix << "response_information: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::server_reference const& t) {
                    std::cout << prefix << "server_reference: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::reason_string const& t) {
                    std::cout << prefix << "reason_string: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::receive_maximum const& t) {
                    std::cout << prefix << "receive_maximum: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::topic_alias_maximum const& t) {
                    std::cout << prefix << "topic_alias_maximum: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::topic_alias const& t) {
                    std::cout << prefix << "topic_alias: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::maximum_qos const& t) {
                    std::cout << prefix << "maximum_qos: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::retain_available const& t) {
                    std::cout << prefix << "retain_available: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::user_property const& t) {
                    std::cout << prefix << "user_property: " << t.key() << ":" << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::maximum_packet_size const& t) {
                    std::cout << prefix << "maximum_packet_size: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::wildcard_subscription_available const& t) {
                    std::cout << prefix << "wildcard_subscription_available: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::subscription_identifier_available const& t) {
                    std::cout << prefix << "subscription_identifier_available: " << t.val() << std::endl;
                },
                [&](MQTT_NS::v5::property::shared_subscription_available const& t) {
                    std::cout << prefix << "shared_subscription_available: " << t.val() << std::endl;
                },
                [&](auto&& ...) {
                    BOOST_ASSERT(false);
                }
            ),
            p
        );
    }
}

inline void print_menu() {
    std::cout << "=== Enter Command ===" << std::endl;
    std::cout << "  subscribe    topic [qos = 0] [nl] [rap] [retain handling] [sub id]" << std::endl;
    std::cout << "  unsubscribe  topic" << std::endl;
    std::cout << "  publish      topic data [qos = 0] [retain]" << std::endl;
    std::cout << "  quit" << std::endl;
    std::cout << "---------------------" << std::endl;
    std::cout << "> " << std::flush;
}

inline
std::tuple<MQTT_NS::subscribe_options, MQTT_NS::v5::properties>
get_opts_props(std::stringstream& ss) {
    std::uint16_t qos = 0;
    ss >> qos;
    std::uint16_t rh = 0;
    std::uint16_t rap = 0;
    std::uint16_t nl = 0;
    std::uint16_t sub_id = 0;

    MQTT_NS::v5::properties props = MQTT_NS::v5::properties{};

    if (ss >> nl) {
        nl = std::uint16_t(nl << 2);
        if (ss >> rap) {
            rap = std::uint16_t(rap << 3);
            if (ss >> rh) {
                rh = std::uint16_t(rh << 4);
                if (ss >> sub_id) {
                    props.emplace_back(MQTT_NS::v5::property::subscription_identifier(sub_id));
                }
            }
        }
    }

    auto opts = MQTT_NS::subscribe_options(
                static_cast<MQTT_NS::qos>(qos) |
                static_cast<MQTT_NS::nl>(nl) |
                static_cast<MQTT_NS::rap>(rap) |
                static_cast<MQTT_NS::retain_handling>(rh)
            );

    return std::make_tuple(opts, MQTT_NS::force_move(props));
}


template <typename Endpoint>
inline void console_input_handler(
    boost::system::error_code const& ec,
    std::size_t /*len*/,
    boost::asio::posix::stream_descriptor& console_input,
    boost::asio::streambuf& buf,
    Endpoint& ep) {

    if (ec) {
        ep.disconnect();
        std::cerr << "console input error ec:" << ec.message() << std::endl;
        return;
    }

    std::istream is(&buf);
    std::string line;

    std::getline(is, line);

    std::stringstream ss(line);
    std::string cmd;

    bool quit = false;
    auto after_proc = MQTT_NS::unique_scope_guard(
        [&] {
            if (quit) return;
            boost::asio::async_read_until(
                console_input,
                buf,
                '\n',
                [&console_input, &buf, &ep](
                    boost::system::error_code const& ec,
                    std::size_t len) {
                    console_input_handler(ec, len, console_input, buf, ep);
                });
        }
    );
    if (ss >> cmd) {
        if (cmd == "subscribe") {
            std::string topic;
            if (ss >> topic) {
                auto opts_props = get_opts_props(ss);
                ep.subscribe(
                    MQTT_NS::force_move(topic),
                    std::get<0>(opts_props),
                    MQTT_NS::force_move(std::get<1>(opts_props))
                );
            }
            return;
        }
        if (cmd == "unsubscribe") {
            std::string topic;
            if (ss >> topic) {
                ep.unsubscribe(
                    MQTT_NS::force_move(topic)
                );
            }
            return;
        }
        if (cmd == "publish") {
            std::string topic;
            std::string payload;
            if (ss >> topic >> payload) {
                if (payload == "\"\"") payload.clear();
                std::uint16_t qos = 0;
                MQTT_NS::retain retain = MQTT_NS::retain::no;
                if (ss >> qos) {
                    std::string retain_string;
                    if (ss >> retain_string) {
                        if (retain_string == "retain") retain = MQTT_NS::retain::yes;
                    }
                }
                MQTT_NS::publish_options opts;
                opts |= MQTT_NS::qos(qos);
                opts |= retain;
                ep.publish(
                    MQTT_NS::allocate_buffer(topic),
                    MQTT_NS::allocate_buffer(payload),
                    opts,
                    MQTT_NS::v5::properties{}
                );
            }
            return;
        }
        if (cmd == "quit") {
            quit = true;
            ep.disconnect();
            return;
        }
        std::cout << "wrong command." << std::endl;
        print_menu();
    }
}

namespace po = boost::program_options;

int main(int argc, char* argv[]) {
    try {
        boost::program_options::options_description desc;

        boost::program_options::options_description general_desc("General options");
        general_desc.add_options()
            ("help", "produce help message")
            (
                "cfg",
                boost::program_options::value<std::string>()->default_value("cli.conf"),
                "Load configuration file"
            )
            (
                "host",
                boost::program_options::value<std::string>(),
                "mqtt broker's hostname to connect"
            )
            (
                "port",
                boost::program_options::value<std::uint16_t>()->default_value(1883),
                "mqtt broker's port to connect"
            )
            (
                "protocol",
                boost::program_options::value<std::string>()->default_value("mqtt"),
                "mqtt mqtts ws wss"
            )
            (
                "mqtt_version",
                boost::program_options::value<std::string>()->default_value("v5"),
                "MQTT version v5 or v3.1.1"
            )
            (
                "clean_start",
                boost::program_options::value<bool>()->default_value(true),
                "set clean_start flag to client"
            )
            (
                "sei",
                boost::program_options::value<std::uint32_t>()->default_value(0),
                "set session expiry interval to client"
            )
            (
                "username",
                boost::program_options::value<std::string>(),
                "username for all clients"
            )
            (
                "password",
                boost::program_options::value<std::string>(),
                "password for all clients"
            )
            (
                 "client_id",
                 po::value<std::string>(),
                 "(optional) client_id"
            )
            (
                "verify_file",
                boost::program_options::value<std::string>(),
                "CA Certificate file to verify server certificate for mqtts and wss connections"
            )
            (
                "certificate",
                boost::program_options::value<std::string>(),
                "Client certificate (chain) file"
            )
            (
                "private_key",
                boost::program_options::value<std::string>(),
                "Client certificate key file"
            )
            (
                "ws_path",
                boost::program_options::value<std::string>(),
                "Web-Socket path for ws and wss connections"
            )
#if defined(MQTT_USE_LOG)
            (
                "verbose",
                boost::program_options::value<unsigned int>()->default_value(1),
                "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace"
            )
#endif // defined(MQTT_USE_LOG)
            ;


        desc.add(general_desc);

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);

        std::string config_file = vm["cfg"].as<std::string>();
        if (!config_file.empty()) {
            std::ifstream input(vm["cfg"].as<std::string>());
            if (input.good()) {
                boost::program_options::store(boost::program_options::parse_config_file(input, desc), vm);
            } else
            {
                std::cerr << "Configuration file '"
                          << config_file
                          << "' not found, not use configuration file." << std::endl;
            }
        }

        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

        std::cout << "Set options:" << std::endl;
        for (auto const& e : vm) {
            std::cout << boost::format("  %-16s") % e.first.c_str() << " : ";
            if (auto p = boost::any_cast<std::string>(&e.second.value())) {
                if (e.first.c_str() == std::string("password")) {
                    std::cout << "********";
                }
                else {
                    std::cout << *p;
                }
            }
            else if (auto p = boost::any_cast<std::size_t>(&e.second.value())) {
                std::cout << *p;
            }
            else if (auto p = boost::any_cast<std::uint32_t>(&e.second.value())) {
                std::cout << *p;
            }
            else if (auto p = boost::any_cast<std::uint16_t>(&e.second.value())) {
                std::cout << *p;
            }
            else if (auto p = boost::any_cast<unsigned int>(&e.second.value())) {
                std::cout << *p;
            }
            else if (auto p = boost::any_cast<bool>(&e.second.value())) {
                std::cout << std::boolalpha << *p;
            }
            std::cout << std::endl;
        }


#if defined(MQTT_USE_LOG)
        switch (vm["verbose"].as<unsigned int>()) {
        case 5:
            MQTT_NS::setup_log(MQTT_NS::severity_level::trace);
            break;
        case 4:
            MQTT_NS::setup_log(MQTT_NS::severity_level::debug);
            break;
        case 3:
            MQTT_NS::setup_log(MQTT_NS::severity_level::info);
            break;
        case 2:
            MQTT_NS::setup_log(MQTT_NS::severity_level::warning);
            break;
        default:
            MQTT_NS::setup_log(MQTT_NS::severity_level::error);
            break;
        case 0:
            MQTT_NS::setup_log(MQTT_NS::severity_level::fatal);
            break;
        }
#else
        MQTT_NS::setup_log();
#endif

        if (!vm.count("host")) {
            std::cerr << "host must be set" << std::endl;
            return -1;
        }

        auto username =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("username")) {
                    return vm["username"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();
        auto password =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("password")) {
                    return vm["password"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();
        auto client_id =
            [&] () -> std::string {
                if (vm.count("client_id")) {
                    return vm["client_id"].as<std::string>();
                }
                return std::string();
            } ();
        auto verify_file =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("verify_file")) {
                    return vm["verify_file"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();
        auto certificate =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("certificate")) {
                    return vm["certificate"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();
        auto private_key =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("private_key")) {
                    return vm["private_key"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();
        auto ws_path =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("ws_path")) {
                    return vm["ws_path"].as<std::string>();
                }
                return MQTT_NS::nullopt;
            } ();

        auto host = vm["host"].as<std::string>();
        auto port = vm["port"].as<std::uint16_t>();
        auto protocol = vm["protocol"].as<std::string>();
        auto clean_start = vm["clean_start"].as<bool>();
        auto sei = vm["sei"].as<std::uint32_t>();
        auto mqtt_version = vm["mqtt_version"].as<std::string>();

        MQTT_NS::protocol_version version =
            [&] {
                if (mqtt_version == "v5" || mqtt_version == "5" || mqtt_version == "v5.0" || mqtt_version == "5.0") {
                    return MQTT_NS::protocol_version::v5;
                }
                else if (mqtt_version == "v3.1.1" || mqtt_version == "3.1.1") {
                    return MQTT_NS::protocol_version::v3_1_1;
                }
                else {
                    std::cerr << "invalid mqtt_version:" << mqtt_version << " it should be v5 or v3.1.1" << std::endl;
                    return MQTT_NS::protocol_version::undetermined;
                }
            } ();

        if (version != MQTT_NS::protocol_version::v5 &&
            version != MQTT_NS::protocol_version::v3_1_1) {
            return -1;
        }

        boost::asio::io_context ioc;
        boost::asio::streambuf buf;
        /* 0 means stdin */
        boost::asio::posix::stream_descriptor console_input(ioc, 0);

        auto setup = [&](auto& client) {
            using packet_id_t = typename std::remove_reference_t<decltype(client)>::packet_id_t;

            if (username) client.set_user_name(MQTT_NS::force_move(*username));
            if (password) client.set_password(MQTT_NS::force_move(*password));
            client.set_client_id(MQTT_NS::force_move(client_id));
            client.set_clean_start(clean_start);

            auto publish_handler =
                []
                (
                    MQTT_NS::optional<packet_id_t> packet_id,
                    MQTT_NS::publish_options pubopts,
                    MQTT_NS::buffer topic_name,
                    MQTT_NS::buffer contents,
                    MQTT_NS::v5::properties props) {

                    std::cout << "<   topic    :" << topic_name << std::endl;
                    if (packet_id) {
                        std::cout << "<   packet_id:" << *packet_id << std::endl;
                    }
                    std::cout << "<   qos      :" << pubopts.get_qos() << std::endl;
                    std::cout << "<   retain   :" << pubopts.get_retain() << std::endl;
                    std::cout << "<   dup      :" << pubopts.get_dup() << std::endl;
                    std::cout << "<   payload  :";
                    for (char c : contents) {
                        switch (c) {
                        case '\\':
                            std::cout << "\\\\";
                            break;
                        case '"':
                            std::cout << "\\\"";
                            break;
                        case '\a':
                            std::cout << "\\a";
                            break;
                        case '\b':
                            std::cout << "\\b";
                            break;
                        case '\f':
                            std::cout << "\\f";
                            break;
                        case '\n':
                            std::cout << "\\n";
                            break;
                        case '\r':
                            std::cout << "\\r";
                            break;
                        case '\t':
                            std::cout << "\\t";
                            break;
                        case '\v':
                            std::cout << "\\v";
                            break;
                        default: {
                            unsigned int code = static_cast<unsigned int>(c);
                            if (code < 0x20 || code >= 0x7f) {
                                std::ios::fmtflags flags(std::cout.flags());
                                std::cout << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (code & 0xff);
                                std::cout.flags(flags);
                            }
                            else {
                                std::cout << c;
                            }
                        } break;
                        }
                    }
                    std::cout << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    print_menu();
                    return true;
                };

            client.set_connack_handler(
                [&]
                (bool sp, MQTT_NS::connect_return_code connack_return_code) {
                    std::cout << "< connack (v3.1.1)" << std::endl;
                    std::cout << "<   return_code:" << connack_return_code << std::endl;
                    std::cout << "<   session_present:" << sp << std::endl;
                    if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                        print_menu();
                        boost::asio::async_read_until(
                            console_input,
                            buf,
                            '\n',
                            [&console_input, &buf, &client]
                            (
                                boost::system::error_code const& ec,
                                std::size_t len
                            ) {
                                console_input_handler(ec, len, console_input, buf, client);
                            }
                        );
                    }
                    return true;
                }
            );
            client.set_v5_connack_handler(
                [&]
                (bool sp, MQTT_NS::v5::connect_reason_code reason_code, MQTT_NS::v5::properties props) {
                    std::cout << "< connack (v5)" << std::endl;
                    std::cout << "<   reason_code:" << reason_code << std::endl;
                    std::cout << "<   session_present:" << sp << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    if (reason_code == MQTT_NS::v5::connect_reason_code::success) {
                        print_menu();
                        boost::asio::async_read_until(
                            console_input,
                            buf,
                            '\n',
                            [&console_input, &buf, &client]
                            (
                                boost::system::error_code const& ec,
                                std::size_t len
                            ) {
                                console_input_handler(ec, len, console_input, buf, client);
                            }
                        );
                    }
                    return true;
                }
            );
            client.set_publish_handler(
                [&]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents) {
                    std::cout << "< publish (v3.1.1)"  << std::endl;
                    return
                        publish_handler(
                            packet_id,
                            pubopts,
                            topic_name,
                            contents,
                            MQTT_NS::v5::properties{}
                        );
                }
            );
            client.set_v5_publish_handler(
                [&]
                (MQTT_NS::optional<packet_id_t> packet_id,
                 MQTT_NS::publish_options pubopts,
                 MQTT_NS::buffer topic_name,
                 MQTT_NS::buffer contents,
                 MQTT_NS::v5::properties props) {
                    std::cout << "< publish (v5)"  << std::endl;
                    return
                        publish_handler(
                            packet_id,
                            pubopts,
                            topic_name,
                            contents,
                            MQTT_NS::force_move(props)
                        );
                }
            );
            client.set_puback_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "< puback (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    return true;
                }
            );
            client.set_v5_puback_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, MQTT_NS::v5::puback_reason_code reason_code, MQTT_NS::v5::properties props){
                    std::cout << "< puback (v5)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    std::cout << "<   reason_code:" << reason_code << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    return true;
                }
            );
            client.set_pubrec_handler(
                []
                (packet_id_t packet_id){
                    std::cout << "< pubrec (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    return true;
                }
            );
            client.set_v5_pubrec_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, MQTT_NS::v5::pubrec_reason_code reason_code, MQTT_NS::v5::properties props){
                    std::cout << "< pubrec (v5)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    std::cout << "<   reason_code:" << reason_code << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    return true;
                }
            );
            client.set_pubrel_handler(
                []
                (packet_id_t packet_id){
                    std::cout << "< pubrel (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    return true;
                }
            );
            client.set_v5_pubrel_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, MQTT_NS::v5::pubrel_reason_code reason_code, MQTT_NS::v5::properties props){
                    std::cout << "< pubrel (v5)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    std::cout << "<   reason_code:" << reason_code << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    return true;
                }
            );
            client.set_pubcomp_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "< pubcomp (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    return true;
                }
            );
            client.set_v5_pubcomp_handler( // use v5 handler
                [&]
                (packet_id_t packet_id, MQTT_NS::v5::pubcomp_reason_code reason_code, MQTT_NS::v5::properties props){
                    std::cout << "< pubcomp (v5)" << std::endl;
                    std::cout << "<   packet_id:" << packet_id << std::endl;
                    std::cout << "<   reason_code:" << reason_code << std::endl;
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    return true;
                }
            );
            client.set_suback_handler(
                [&]
                (packet_id_t packet_id, std::vector<MQTT_NS::suback_return_code> results){
                    std::cout << "< suback (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id: " << packet_id << std::endl;
                    std::cout << "<   return_code:" << packet_id << std::endl;
                    for (auto const& e: results) {
                        std::cout << "<   " << e << std::endl;
                    }
                    print_menu();
                    return true;
                }
            );
            client.set_v5_suback_handler(
                [&]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::v5::suback_reason_code> reasons,
                 MQTT_NS::v5::properties props){
                    std::cout << "< suback (v5)" << std::endl;
                    std::cout << "<   packet_id: " << packet_id << std::endl;
                    std::cout << "<   reason_code:" << packet_id << std::endl;
                    for (auto const& e: reasons) {
                        std::cout << "<     " << e << std::endl;
                    }
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    print_menu();
                    return true;
                }
            );
            client.set_unsuback_handler(
                [&]
                (packet_id_t packet_id){
                    std::cout << "< unsuback (v3.1.1)" << std::endl;
                    std::cout << "<   packet_id: " << packet_id << std::endl;
                    print_menu();
                    return true;
                }
            );
            client.set_v5_unsuback_handler(
                [&]
                (packet_id_t packet_id,
                 std::vector<MQTT_NS::v5::unsuback_reason_code> reasons,
                 MQTT_NS::v5::properties props){
                    std::cout << "< unsuback (v5)" << std::endl;
                    std::cout << "<   packet_id: " << packet_id << std::endl;
                    std::cout << "<   unsuback_reason_code:" << std::endl;
                    for (auto const& e: reasons) {
                        std::cout << "<     " << e << std::endl;
                    }
                    std::cout << "<   props:" << std::endl;
                    print_props("<     ", props);
                    print_menu();
                    return true;
                }
            );

            client.set_close_handler(
                []() {
                    std::cout << "< closed." << std::endl;
                });
            client.set_error_handler(
                [](boost::system::error_code const& ec) {
                    std::cout << "< error:" << ec.message() << std::endl;
                });
            MQTT_NS::v5::properties props;
            if (sei != 0) {
                props.emplace_back(
                    MQTT_NS::v5::property::session_expiry_interval(sei)
                );
            }
            client.connect(MQTT_NS::force_move(props));
            ioc.run();
        };

        if (protocol == "mqtt") {
            auto client = MQTT_NS::make_sync_client(
                ioc,
                host,
                port,
                version
            );
            setup(*client);
        }
        else if (protocol == "mqtts") {
#if defined(MQTT_USE_TLS)
            auto client = MQTT_NS::make_tls_sync_client(
                ioc,
                host,
                port,
                version
            );
            if (verify_file) {
                client->get_ssl_context().load_verify_file(*verify_file);
            }
            if (certificate) {
                client->get_ssl_context().use_certificate_chain_file(*certificate);
            }
            if (private_key) {
                client->get_ssl_context().use_private_key_file(*private_key, boost::asio::ssl::context::pem);
            }
            setup(*client);
            return 0;
#else  // defined(MQTT_USE_TLS)
            std::cout << "MQTT_USE_TLS compiler option is required" << std::endl;
            return -1;
#endif // defined(MQTT_USE_TLS)
        }
        else if (protocol == "ws") {
#if defined(MQTT_USE_WS)
            auto client = MQTT_NS::make_sync_client_ws(
                ioc,
                host,
                port,
                ws_path ? ws_path.value() : std::string(),
                version
            );
            setup(*client);
            return 0;
#else  // defined(MQTT_USE_WS)
            std::cout << "MQTT_USE_WS compiler option is required" << std::endl;
            return -1;
#endif // defined(MQTT_USE_WS)
        }
        else if (protocol == "wss") {
#if defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
            auto client = MQTT_NS::make_tls_sync_client_ws(
                ioc,
                host,
                port,
                ws_path ? ws_path.value() : std::string(),
                version
            );
            if (verify_file) {
                client->get_ssl_context().load_verify_file(*verify_file);
            }
            if (certificate) {
                client->get_ssl_context().use_certificate_chain_file(*certificate);
            }
            if (private_key) {
                client->get_ssl_context().use_private_key_file(*private_key, boost::asio::ssl::context::pem);
            }
            setup(*client);
            return 0;
#else  // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
            std::cout << "MQTT_USE_TLS and MQTT_USE_WS compiler option are required" << std::endl;
            return -1;
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
        }
        else {
            std::cerr << "invalid protocol:" << protocol << " it should be mqtt, mqtts, ws, or wss" << std::endl;
            return -1;
        }
    }
    catch (std::exception const &e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }
}
