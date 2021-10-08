// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <mqtt/config.hpp>
#include <mqtt/setup_log.hpp>
#include <mqtt/async_client.hpp>

#include <thread>
#include <fstream>

#include <boost/program_options.hpp>
#include <boost/format.hpp>


namespace as = boost::asio;

int main(int argc, char **argv) {
    try {
        boost::program_options::options_description desc;

        constexpr std::size_t min_payload = 15;
        std::string payload_size_desc =
            "payload bytes. must be greater than " + std::to_string(min_payload);

        boost::program_options::options_description general_desc("General options");
        general_desc.add_options()
            ("help", "produce help message")
            (
                "cfg",
                boost::program_options::value<std::string>()->default_value("bench.conf"),
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
                "qos",
                boost::program_options::value<unsigned int>()->default_value(0),
                "QoS 0, 1, or 2"
            )
            (
                "payload_size",
                boost::program_options::value<std::size_t>()->default_value(1024),
                payload_size_desc.c_str()
            )
            (
                "compare",
                boost::program_options::value<bool>()->default_value(false),
                "compare send/receive payloads"
            )
            (
                "retain",
                boost::program_options::value<bool>()->default_value(false),
                "set retain flag to publish"
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
                "times",
                boost::program_options::value<std::size_t>()->default_value(1000),
                "number of publishes for each client"
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
                "cid_prefix",
                boost::program_options::value<std::string>()->default_value(""),
                "client_id prefix. client_id is cid_prefix00000000 cid_prefix00000001 ..."
            )
            (
                "topic_prefix",
                boost::program_options::value<std::string>()->default_value(""),
                "topic_id prefix. topic is topic_prefix00000000 topic_prefix00000001 ..."
            )
            (
                "limit_ms",
                boost::program_options::value<std::size_t>()->default_value(1000),
                "Output time over message if round trip time is greater than limit_ms"
            )
            (
                "iocs",
                boost::program_options::value<std::size_t>()->default_value(1),
                "Number of io_context. If set 0 then automatically decided by hardware_concurrency()."
            )
            (
                "threads_per_ioc",
                boost::program_options::value<std::size_t>()->default_value(1),
                "Number of worker threads for each io_context."
            )
            (
                "clients",
                boost::program_options::value<std::size_t>()->default_value(1),
                "Number of clients."
            )
            (
                "con_interval_ms",
                boost::program_options::value<std::size_t>()->default_value(10),
                "connect interval (ms)"
            )
            (
                "sub_delay_ms",
                boost::program_options::value<std::size_t>()->default_value(1000),
                "subscribe delay after all connected (ms)"
            )
            (
                "sub_interval_ms",
                boost::program_options::value<std::size_t>()->default_value(10),
                "subscribe interval (ms)"
            )
            (
                "pub_delay_ms",
                boost::program_options::value<std::size_t>()->default_value(1000),
                "publish delay after all subscribed (ms)"
            )
            (
                "pub_interval_ms",
                boost::program_options::value<std::size_t>()->default_value(10),
                "publish interval for each clients (ms)"
            )
            (
                "detail_report",
                boost::program_options::value<bool>()->default_value(false),
                "report for each client's max mid min"
            )
            (
                "pub_idle_count",
                boost::program_options::value<std::size_t>()->default_value(1),
                "ideling publish count. it is useful to ignore authorization cache."
            )
#if defined(MQTT_USE_LOG)
            (
                "verbose",
                boost::program_options::value<unsigned int>()->default_value(1),
                "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace"
            )
#endif // defined(MQTT_USE_LOG)
            (
                "cacert",
                boost::program_options::value<std::string>(),
                "CA Certificate file to verify server certificate for mqtts and wss connections"
            )
            (
                "ws_path",
                boost::program_options::value<std::string>(),
                "Web-Socket path for ws and wss connections"
            )
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
                          << "' not found,  bench doesn't use configuration file." << std::endl;
            }
        }

        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

        std::cout << "Set options:" << std::endl;
        for (auto const& e : vm) {
            std::cout << boost::format("%-16s") % e.first.c_str() << " : ";
            if (auto p = boost::any_cast<std::string>(&e.second.value())) {
                std::cout << *p;
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

        auto detail_report = vm["detail_report"].as<bool>();
        auto host = vm["host"].as<std::string>();
        auto port = vm["port"].as<std::uint16_t>();
        auto protocol = vm["protocol"].as<std::string>();
        auto mqtt_version = vm["mqtt_version"].as<std::string>();
        auto qos = static_cast<MQTT_NS::qos>(vm["qos"].as<unsigned int>());
        auto retain =
            [&] () -> MQTT_NS::retain {
                if (vm["retain"].as<bool>()) {
                    return MQTT_NS::retain::yes;
                }
                return MQTT_NS::retain::no;
            } ();
        auto clean_start = vm["clean_start"].as<bool>();
        auto sei = vm["sei"].as<std::uint32_t>();
        auto payload_size = vm["payload_size"].as<std::size_t>();
        if (payload_size <= min_payload) {
            std::cout
                << "payload_size must be greater than "
                << std::to_string(min_payload)
                << ". payload_size:" << payload_size
                << std::endl;
            return -1;
        }
        auto compare = vm["compare"].as<bool>();

        auto clients = vm["clients"].as<std::size_t>();
        auto times = vm["times"].as<std::size_t>();
        if (times == 0) {
            std::cout << "times must be greater than 0" << std::endl;
            return -1;
        }
        auto pub_idle_count = vm["pub_idle_count"].as<std::size_t>();
        times += pub_idle_count;
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
        auto cid_prefix = vm["cid_prefix"].as<std::string>();
        auto topic_prefix = vm["cid_prefix"].as<std::string>();

        auto cacert =
            [&] () -> MQTT_NS::optional<std::string> {
                if (vm.count("cacert")) {
                    return vm["cacert"].as<std::string>();
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

        auto limit_ms = vm["limit_ms"].as<std::size_t>();

        auto con_interval_ms = vm["con_interval_ms"].as<std::size_t>();
        auto sub_delay_ms = vm["sub_delay_ms"].as<std::size_t>();
        auto sub_interval_ms = vm["sub_interval_ms"].as<std::size_t>();
        auto pub_delay_ms = vm["pub_delay_ms"].as<std::size_t>();
        auto pub_interval_ms = vm["pub_interval_ms"].as<std::size_t>();

        std::uint64_t pub_interval_us = pub_interval_ms * 1000;
        std::cout << "pub_interval:" << pub_interval_us << " us" << std::endl;
        auto all_interval_us = pub_interval_us / clients;
        std::cout << "all_interval:" << all_interval_us << " us" << std::endl;
        std::cout << (double(1) * 1000 * 1000 / static_cast<double>(all_interval_us)) <<  " publish/sec" << std::endl;
        auto num_of_iocs =
            [&] () -> std::size_t {
                if (vm.count("iocs")) {
                    return vm["iocs"].as<std::size_t>();
                }
                return 1;
            } ();
        if (num_of_iocs == 0) {
            num_of_iocs = std::thread::hardware_concurrency();
            std::cout << "iocs set to auto decide (0). Automatically set to " << num_of_iocs << std::endl;;
        }

        auto threads_per_ioc =
            [&] () -> std::size_t {
                if (vm.count("threads_per_ioc")) {
                    return vm["threads_per_ioc"].as<std::size_t>();
                }
                return 1;
            } ();
        if (threads_per_ioc == 0) {
            threads_per_ioc = std::min(std::size_t(std::thread::hardware_concurrency()), std::size_t(4));
            std::cout << "threads_per_ioc set to auto decide (0). Automatically set to " << threads_per_ioc << std::endl;
        }

        std::cout
            << "iocs:" << num_of_iocs
            << " threads_per_ioc:" << threads_per_ioc
            << " total threads:" << num_of_iocs * threads_per_ioc
            << std::endl;

        std::vector<as::io_context> iocs(num_of_iocs);
        BOOST_ASSERT(!iocs.empty());


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


        auto bench_proc =
            [&](auto& cis) {
                std::atomic<std::size_t> rest_connect{clients};
                std::atomic<std::size_t> rest_sub{clients};
                std::atomic<std::uint64_t> rest_times{times * clients};
                auto sub_proc =
                    [&] {
                        std::this_thread::sleep_for(std::chrono::milliseconds(sub_delay_ms));
                        std::cout << "Subscribe" << std::endl;
                        for (auto& ci : cis) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(sub_interval_ms));
                            ci.c->async_subscribe(
                                topic_prefix + ci.index_str,
                                qos,
                                [&](MQTT_NS::error_code ec) {
                                    if (ec) {
                                        std::cout << "sub error:" << ec.message() << std::endl;
                                    }
                                }
                            );
                        }
                    };

                auto async_wait_pub =
                    [&] (auto& ci) {
                        ci.tim->async_wait(
                            [&] (boost::system::error_code const& ec) {
                                if (ec && ec != as::error::operation_aborted) {
                                    std::cout << "timer error:" << ec.message() << std::endl;
                                }
                                else {
                                    MQTT_NS::publish_options opts = qos | retain;
                                    ci.sent = std::chrono::steady_clock::now();
                                    ci.c->async_publish(
                                        topic_prefix + ci.index_str,
                                        ci.payload(),
                                        opts,
                                        [&](MQTT_NS::error_code ec) {
                                            if (ec) {
                                                std::cout << "pub error:" << ec.message() << std::endl;
                                            }
                                        }
                                    );
                                }
                            }
                        );
                    };

                auto pub_proc =
                    [&] {
                        std::cout << "Publish" << std::endl;
                        std::size_t index = 0;
                        for (auto& ci : cis) {
                            ci.tim->expires_after(
                                std::chrono::milliseconds(pub_delay_ms) +
                                std::chrono::microseconds(all_interval_us) * index++
                            );
                            async_wait_pub(ci);
                        }
                    };

                auto finish_proc =
                    [&] {
                        std::cout << "Report" << std::endl;
                        std::size_t maxmax = 0;
                        std::string maxmax_cid;
                        std::size_t maxmid = 0;
                        std::string maxmid_cid;
                        std::size_t maxmin = 0;
                        std::string maxmin_cid;
                        for (auto& ci : cis) {
                            std::sort(ci.rtt_us.begin(), ci.rtt_us.end());
                            std::string cid = ci.c->get_client_id();
                            std::size_t max = ci.rtt_us.back();
                            std::size_t mid = ci.rtt_us.at(ci.rtt_us.size() / 2);
                            std::size_t min = ci.rtt_us.front();
                            if (maxmax < max) {
                                maxmax = max;
                                maxmax_cid = cid;
                            }
                            if (maxmid < mid) {
                                maxmid = mid;
                                maxmid_cid = cid;
                            }
                            if (maxmin < min) {
                                maxmin = min;
                                maxmin_cid = cid;
                            }
                            if (detail_report) {
                                std::cout
                                    << cid << " :"
                                    << " max:" << boost::format("%+12d") % max << " us | "
                                    << " mid:" << boost::format("%+12d") % mid << " us | "
                                    << " min:" << boost::format("%+12d") % min << " us | "
                                    << std::endl;
                            }
                        }
                        std::cout
                            << "maxmax:" << boost::format("%+12d") % maxmax << " us "
                            << "(" << boost::format("%+8d") % (maxmax / 1000) << " ms ) "
                            << "client_id:" << maxmax_cid << std::endl;
                        std::cout
                            << "maxmid:" << boost::format("%+12d") % maxmid << " us "
                            << "(" << boost::format("%+8d") % (maxmid / 1000) << " ms ) "
                            << "client_id:" << maxmid_cid << std::endl;
                        std::cout
                            << "maxmin:" << boost::format("%+12d") % maxmin << " us "
                            << "(" << boost::format("%+8d") % (maxmin / 1000) << " ms ) "
                            << "client_id:" << maxmin_cid << std::endl;

                        for (auto& ci : cis) {
                            ci.c->async_force_disconnect();
                        }
                        std::cout << "Finish" << std::endl;
                    };

                using packet_id_t = typename std::remove_reference_t<decltype(*cis.front().c)>::packet_id_t;
                auto publish_handler =
                    [&](auto& ci,
                        MQTT_NS::optional<packet_id_t> /*packet_id*/,
                        MQTT_NS::publish_options /*pubopts*/,
                        MQTT_NS::buffer topic_name,
                        MQTT_NS::buffer contents,
                        MQTT_NS::v5::properties /*props*/) {
                        if (ci.idle_count == 0) {
                            auto recv = std::chrono::steady_clock::now();
                            auto dur_us = std::chrono::duration_cast<std::chrono::microseconds>(recv - ci.sent).count();
                            if (static_cast<unsigned long>(dur_us) > limit_ms * 1000) {
                                std::cout << "RTT over " << limit_ms << " ms" << std::endl;
                            }
                            if (compare) {
                                if (contents != ci.payload()) {
                                    std::cout << "received payload doesn't match to sent one" << std::endl;
                                    std::cout << "  send: " << ci.payload() << std::endl;
                                    std::cout << "  recv: " << contents << std::endl;;
                                }
                            }
                            if (topic_name != topic_prefix + ci.index_str) {
                                std::cout << "topic doesn't match" << std::endl;
                                std::cout << "  send: " << topic_prefix + ci.index_str << std::endl;
                                std::cout << "  recv: " << topic_name << std::endl;
                            }
                            ci.rtt_us.emplace_back(dur_us);
                        }
                        else {
                            --ci.idle_count;
                        }

                        ci.tim->cancel();
                        ci.tim->expires_at(
                            ci.tim->expiry() + std::chrono::microseconds(all_interval_us)
                        );
                        if (--ci.times != 0) {
                            async_wait_pub(ci);
                        }
                        if (--rest_times == 0) {
                            finish_proc();
                        }
                        return true;
                    };

                for (auto& ci : cis) {
                    ci.c->set_auto_pub_response(true);
                    ci.c->set_async_operation(true);
                    ci.c->set_clean_start(clean_start);
                    if (username) ci.c->set_user_name(username.value());
                    if (password) ci.c->set_password(password.value());
                    ci.c->set_client_id(cid_prefix + ci.index_str);
                    ci.c->set_connack_handler(
                        [&]
                        (bool /*sp*/, MQTT_NS::connect_return_code connack_return_code) {
                            if (connack_return_code == MQTT_NS::connect_return_code::accepted) {
                                if (--rest_connect == 0) sub_proc();
                            }
                            else {
                                std::cout << "connack error:" << connack_return_code << std::endl;
                            }
                            return true;
                        }
                    );
                    ci.c->set_v5_connack_handler(
                        [&]
                        (bool /*sp*/, MQTT_NS::v5::connect_reason_code reason_code, MQTT_NS::v5::properties /*props*/) {
                            if (reason_code == MQTT_NS::v5::connect_reason_code::success) {
                                if (--rest_connect == 0) sub_proc();
                            }
                            else {
                                std::cout << "connack error:" << reason_code << std::endl;
                            }
                            return true;
                        }
                    );

                    ci.c->set_suback_handler(
                        [&]
                        (packet_id_t /*packet_id*/, std::vector<MQTT_NS::suback_return_code> results) {
                            BOOST_ASSERT(results.size() == 1);
                            if (results.front() == MQTT_NS::suback_return_code::success_maximum_qos_0 ||
                                results.front() == MQTT_NS::suback_return_code::success_maximum_qos_1 ||
                                results.front() == MQTT_NS::suback_return_code::success_maximum_qos_2) {
                                if (--rest_sub == 0) pub_proc();
                            }
                            return true;
                        }
                    );
                    ci.c->set_v5_suback_handler(
                        [&]
                        (packet_id_t /*packet_id*/,
                         std::vector<MQTT_NS::v5::suback_reason_code> reasons,
                         MQTT_NS::v5::properties /*props*/) {
                            BOOST_ASSERT(reasons.size() == 1);
                            if (reasons.front() == MQTT_NS::v5::suback_reason_code::granted_qos_0 ||
                                reasons.front() == MQTT_NS::v5::suback_reason_code::granted_qos_1 ||
                                reasons.front() == MQTT_NS::v5::suback_reason_code::granted_qos_2) {
                                if (--rest_sub == 0) pub_proc();
                            }
                            return true;
                        }
                    );

                    ci.c->set_publish_handler(
                        [&]
                        (MQTT_NS::optional<packet_id_t> packet_id,
                         MQTT_NS::publish_options pubopts,
                         MQTT_NS::buffer topic_name,
                         MQTT_NS::buffer contents) {
                            return publish_handler(ci, packet_id, pubopts, topic_name, contents, MQTT_NS::v5::properties{});
                        }
                    );
                    ci.c->set_v5_publish_handler(
                        [&]
                        (MQTT_NS::optional<packet_id_t> packet_id,
                         MQTT_NS::publish_options pubopts,
                         MQTT_NS::buffer topic_name,
                         MQTT_NS::buffer contents,
                         MQTT_NS::v5::properties props) {
                            return publish_handler(ci, packet_id, pubopts, topic_name, contents, MQTT_NS::force_move(props));
                        }
                    );
                }

                for (auto& ci : cis) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(con_interval_ms));
                    MQTT_NS::v5::properties props;
                    if (sei != 0) {
                        props.emplace_back(
                            MQTT_NS::v5::property::session_expiry_interval(sei)
                        );
                    }
                    ci.c->async_connect(
                        MQTT_NS::force_move(props),
                        [&](MQTT_NS::error_code ec) {
                            if (ec) {
                                std::cerr << "async_connect error: " << ec.message() << std::endl;
                            }
                            ci.init_timer(ci.c->get_executor());
                        }
                    );
                }
                std::vector<std::thread> ths;
                ths.reserve(num_of_iocs * threads_per_ioc);
                for (auto& ioc : iocs) {
                    for (std::size_t i = 0; i != threads_per_ioc; ++i) {
                        ths.emplace_back(
                            [&] {
                                ioc.run();
                            }
                        );
                    }
                }
                for (auto& th : ths) th.join();
            };

        std::cout << "Prepare clients" << std::endl;
        std::cout << "  protocol:" << protocol << std::endl;

        struct client_info_base {
            client_info_base(std::size_t index, std::size_t payload_size, std::size_t times, std::size_t idle_count)
                :index_str{(boost::format("%08d") % index).str()},
                 times{times},
                 idle_count{idle_count}
            {
                payload_str.resize(payload_size);
                auto it = payload_str.begin() + min_payload;
                auto end = payload_str.end();
                char c = 'A';
                for (; it != end; ++it) {
                    *it = c++;
                    if (c == 'Z') c = 'A';
                }
            }

            std::string payload() const {
                std::string ret = payload_str;
                auto it = ret.begin() + min_payload;
                auto variable = (boost::format("%s%08d") %index_str % times).str();
                std::copy(variable.begin(), variable.end(), it);
                return ret;
            }

#if BOOST_VERSION < 107400 || defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
            using executor_t =  as::executor;
#else  // BOOST_VERSION < 107400 || defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)
            using executor_t =  as::any_io_executor;
#endif // BOOST_VERSION < 107400 || defined(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT)

            void init_timer(executor_t exe) {
                tim = std::make_shared<as::steady_timer>(exe);
            }

            std::string index_str;
            std::string payload_str;
            std::size_t times;
            std::size_t idle_count;
            std::chrono::steady_clock::time_point sent;
            std::chrono::steady_clock::time_point next;
            std::vector<std::size_t> rtt_us;
            std::shared_ptr<as::steady_timer> tim;
        };

        if (protocol == "mqtt") {
            using client_t = decltype(
                MQTT_NS::make_async_client(
                    std::declval<as::io_context&>(),
                    host,
                    port,
                    version
                )
            );
            struct client_info : client_info_base {
                client_info(client_t c, std::size_t index, std::size_t payload_size, std::size_t times, std::size_t idle_count)
                    :client_info_base(index, payload_size, times, idle_count),
                     c{MQTT_NS::force_move(c)}

                {
                }
                client_t c;
            };

            std::vector<client_info> cis;
            cis.reserve(clients);
            for (std::size_t i = 0; i != clients; ++i) {
                cis.emplace_back(
                    MQTT_NS::make_async_client(
                        iocs.at(i % num_of_iocs),
                        host,
                        port,
                        version
                    ),
                    i,
                    payload_size,
                    times,
                    pub_idle_count
                );
            }
            bench_proc(cis);
        }
        else if (protocol == "mqtts") {
#if defined(MQTT_USE_TLS)
            using client_t = decltype(
                MQTT_NS::make_tls_async_client(
                    std::declval<as::io_context&>(),
                    host,
                    port,
                    version
                )
            );
            struct client_info : client_info_base {
                client_info(client_t c, std::size_t index, std::size_t payload_size, std::size_t times, std::size_t idle_count)
                    :client_info_base(index, payload_size, times, idle_count),
                     c{MQTT_NS::force_move(c)}
                {
                }
                client_t c;
            };

            std::vector<client_info> cis;
            cis.reserve(clients);
            for (std::size_t i = 0; i != clients; ++i) {
                cis.emplace_back(
                    MQTT_NS::make_tls_async_client(
                        iocs.at(i % num_of_iocs),
                        host,
                        port,
                        version
                    ),
                    i,
                    payload_size,
                    times,
                    pub_idle_count
                );
                if (cacert) {
                    cis.back().c->get_ssl_context().load_verify_file(cacert.value());
                }
            }
            bench_proc(cis);
            return 0;
#else  // defined(MQTT_USE_TLS)
            std::cout << "MQTT_USE_TLS compiler option is required" << std::endl;
            return -1;
#endif // defined(MQTT_USE_TLS)
        }
        else if (protocol == "ws") {
#if defined(MQTT_USE_WS)
            using client_t = decltype(
                MQTT_NS::make_async_client_ws(
                    std::declval<as::io_context&>(),
                    host,
                    port,
                    "",
                    version
                )
            );
            struct client_info : client_info_base {
                client_info(client_t c, std::size_t index, std::size_t payload_size, std::size_t times, std::size_t idle_count)
                    :client_info_base(index, payload_size, times, idle_count),
                     c{MQTT_NS::force_move(c)}
                {
                }
                client_t c;
            };

            std::vector<client_info> cis;
            cis.reserve(clients);
            for (std::size_t i = 0; i != clients; ++i) {
                cis.emplace_back(
                    MQTT_NS::make_async_client_ws(
                        iocs.at(i % num_of_iocs),
                        host,
                        port,
                        ws_path ? ws_path.value() : std::string(),
                        version
                    ),
                    i,
                    payload_size,
                    times,
                    pub_idle_count
                );
            }
            bench_proc(cis);
#else  // defined(MQTT_USE_WS)
            std::cout << "MQTT_USE_WS compiler option is required" << std::endl;
            return -1;
#endif // defined(MQTT_USE_WS)
        }
        else if (protocol == "wss") {
#if defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
            using client_t = decltype(
                MQTT_NS::make_tls_async_client_ws(
                    std::declval<as::io_context&>(),
                    host,
                    port,
                    "",
                    version
                )
            );
            struct client_info : client_info_base {
                client_info(client_t c, std::size_t index, std::size_t payload_size, std::size_t times, std::size_t idle_count)
                    :client_info_base(index, payload_size, times, idle_count),
                     c{MQTT_NS::force_move(c)}
                {
                }
                client_t c;
            };

            std::vector<client_info> cis;
            cis.reserve(clients);
            for (std::size_t i = 0; i != clients; ++i) {
                cis.emplace_back(
                    MQTT_NS::make_tls_async_client_ws(
                        iocs.at(i % num_of_iocs),
                        host,
                        port,
                        ws_path ? ws_path.value() : std::string(),
                        version
                    ),
                    i,
                    payload_size,
                    times,
                    pub_idle_count
                );
                if (cacert) {
                    cis.back().c->get_ssl_context().load_verify_file(cacert.value());
                }
            }
            bench_proc(cis);
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


    } catch(std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
