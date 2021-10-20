// Copyright Takatoshi Kondo 2020
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#define BOOST_UUID_FORCE_AUTO_LINK
#include <mqtt/config.hpp>
#include <mqtt/setup_log.hpp>
#include <mqtt/broker/broker.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>

#include <fstream>
#include <algorithm>

namespace as = boost::asio;

using con_t = MQTT_NS::server<>::endpoint_t;
using con_sp_t = std::shared_ptr<con_t>;


class server_no_tls {
public:
    server_no_tls(
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        MQTT_NS::broker::broker_t& b,
        uint16_t port
    )
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), port
            ),
            ioc_accept,
            MQTT_NS::force_move(ioc_con_getter),
            [](auto& acceptor) {
                acceptor.set_option(as::ip::tcp::acceptor::reuse_address(true));
            }
        ), b_(b) {
        server_.set_error_handler(
            [](MQTT_NS::error_code /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](con_sp_t spep) {
                b_.handle_accept(MQTT_NS::force_move(spep));
            }
        );

        server_.listen();
    }

    MQTT_NS::broker::broker_t& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

private:
    MQTT_NS::server<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#if defined(MQTT_USE_TLS)

class server_tls {
public:
    server_tls(
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        boost::asio::ssl::context&& ctx,
        MQTT_NS::broker::broker_t& b,
        uint16_t port
    )
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), port
            ),
            MQTT_NS::force_move(ctx),
            ioc_accept,
            MQTT_NS::force_move(ioc_con_getter),
            [](auto& acceptor) {
                acceptor.set_option(as::ip::tcp::acceptor::reuse_address(true));
            }
        ), b_(b) {
        server_.set_error_handler(
            [](MQTT_NS::error_code /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](std::shared_ptr<MQTT_NS::server_tls<>::endpoint_t> spep) {
                b_.handle_accept(MQTT_NS::force_move(spep));
            }
        );

        server_.listen();
    }

    MQTT_NS::broker::broker_t& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    MQTT_NS::tls::context& get_ssl_context() {
        return server_.get_ssl_context();
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    MQTT_NS::tls::context const& get_ssl_context() const {
        return server_.get_ssl_context();
    }

private:
    MQTT_NS::server_tls<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS)

class server_no_tls_ws {
public:
    server_no_tls_ws(
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        MQTT_NS::broker::broker_t& b,
        uint16_t port)
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(), port
            ),
            ioc_accept,
            MQTT_NS::force_move(ioc_con_getter),
            [](auto& acceptor) {
                acceptor.set_option(as::ip::tcp::acceptor::reuse_address(true));
            }
        ), b_(b) {
        server_.set_error_handler(
            [](MQTT_NS::error_code /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](std::shared_ptr<MQTT_NS::server_ws<>::endpoint_t> spep) {
                b_.handle_accept(MQTT_NS::force_move(spep));
            }
        );

        server_.listen();
    }

    MQTT_NS::broker::broker_t& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

private:
    MQTT_NS::server_ws<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#if defined(MQTT_USE_TLS)

class server_tls_ws {
public:
    server_tls_ws(
        as::io_context& ioc_accept,
        std::function<as::io_context&()> ioc_con_getter,
        boost::asio::ssl::context&& ctx,
        MQTT_NS::broker::broker_t& b,
        uint16_t port
    )
        : server_(
            as::ip::tcp::endpoint(
                as::ip::tcp::v4(),
                port
            ),
            MQTT_NS::force_move(ctx),
            ioc_accept,
            MQTT_NS::force_move(ioc_con_getter),
            [](auto& acceptor) {
                acceptor.set_option(as::ip::tcp::acceptor::reuse_address(true));
            }
        ), b_(b) {
        server_.set_error_handler(
            [](MQTT_NS::error_code /*ec*/) {
            }
        );

        server_.set_accept_handler(
            [&](std::shared_ptr<MQTT_NS::server_tls_ws<>::endpoint_t> spep) {
                b_.handle_accept(MQTT_NS::force_move(spep));
            }
        );

        server_.listen();
    }

    MQTT_NS::broker::broker_t& broker() const {
        return b_;
    }

    void close() {
        server_.close();
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    MQTT_NS::tls::context& get_ssl_context() {
        return server_.get_ssl_context();
    }

    /**
     * @brief Get boost asio ssl context.
     * @return ssl context
     */
    MQTT_NS::tls::context const& get_ssl_context() const {
        return server_.get_ssl_context();
    }

private:
    MQTT_NS::server_tls_ws<> server_;
    MQTT_NS::broker::broker_t& b_;
};

#endif // defined(MQTT_USE_TLS)
#endif // defined(MQTT_USE_WS)


#if defined(MQTT_USE_TLS)
as::ssl::context init_ctx()
{
    as::ssl::context ctx(as::ssl::context::tlsv12);
    ctx.set_options(
        as::ssl::context::default_workarounds |
        as::ssl::context::single_dh_use);
    return ctx;
}

template<typename Server>
void reload_ctx(Server& server, as::steady_timer& reload_timer,
                std::string const& certificate_filename,
                std::string const& key_filename,
                unsigned int certificate_reload_interval,
                char const* name, bool first_load = true)
{
    MQTT_LOG("mqtt_broker", info) << "Reloading certificates for server " << name;

    if (certificate_reload_interval > 0) {
        reload_timer.expires_after(std::chrono::hours(certificate_reload_interval));
        reload_timer.async_wait(
            [&server, &reload_timer, certificate_filename, key_filename, certificate_reload_interval, name]
            (boost::system::error_code const& e) {

            BOOST_ASSERT(!e || e == as::error::operation_aborted);

            if (!e) {
                reload_ctx(server, reload_timer, certificate_filename, key_filename, certificate_reload_interval, name, false);
            }
        });
    }

    auto context = init_ctx();

    boost::system::error_code ec;
    context.use_certificate_chain_file(certificate_filename, ec);
    if (ec) {
        auto message = "Failed to load certificate file: " + ec.message();
        if (first_load) {
            throw std::runtime_error(message);
        }

        MQTT_LOG("mqtt_broker", warning) << message;
        return;
    }

    context.use_private_key_file(key_filename, as::ssl::context::pem, ec);
    if (ec) {
        auto message = "Failed to load private key file: " + ec.message();
        if (first_load) {
            throw std::runtime_error(message);
        }

        MQTT_LOG("mqtt_broker", warning) << message;
        return;
    }

    server.get_ssl_context() = std::move(context);
}

template<typename Server>
void load_ctx(Server& server, as::steady_timer& reload_timer, boost::program_options::variables_map const& vm, char const* name)
{
    if (vm.count("certificate") == 0 && vm.count("private_key") == 0) {
        throw std::runtime_error("TLS requested but certificate and/or private_key not specified");
    }

    reload_ctx(server, reload_timer,
           vm["certificate"].as<std::string>(),
           vm["private_key"].as<std::string>(),
           vm["certificate_reload_interval"].as<unsigned int>(),
           name, true);
}
#endif // defined(MQTT_USE_TLS)




void run_broker(boost::program_options::variables_map const& vm) {
    try {
        as::io_context timer_ioc;
        MQTT_NS::broker::broker_t b(timer_ioc);

        auto num_of_iocs =
            [&] () -> std::size_t {
                if (vm.count("iocs")) {
                    return vm["iocs"].as<std::size_t>();
                }
                return 1;
            } ();
        if (num_of_iocs == 0) {
            num_of_iocs = std::thread::hardware_concurrency();
            MQTT_LOG("mqtt_broker", info) << "iocs set to auto decide (0). Automatically set to " << num_of_iocs;
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
            MQTT_LOG("mqtt_broker", info) << "threads_per_ioc set to auto decide (0). Automatically set to " << threads_per_ioc;
        }

        MQTT_LOG("mqtt_broker", info)
            << "iocs:" << num_of_iocs
            << " threads_per_ioc:" << threads_per_ioc
            << " total threads:" << num_of_iocs * threads_per_ioc;

        std::string auth_file = vm["auth_file"].as<std::string>();
        if (!auth_file.empty()) {
            MQTT_LOG("mqtt_broker", info)
                << "auth_file:" << auth_file;

            std::ifstream input(auth_file);
            b.get_security().load_json(input);
        }

        as::io_context accept_ioc;

        std::mutex mtx_con_iocs;
        std::vector<as::io_context> con_iocs(num_of_iocs);
        BOOST_ASSERT(!con_iocs.empty());

        std::vector<
            as::executor_work_guard<
                as::io_context::executor_type
            >
        > guard_con_iocs;
        guard_con_iocs.reserve(con_iocs.size());
        for (auto& con_ioc : con_iocs) {
            guard_con_iocs.emplace_back(con_ioc.get_executor());
        }

        auto con_iocs_it = con_iocs.begin();

        auto con_ioc_getter =
            [&mtx_con_iocs, &con_iocs, &con_iocs_it]() -> as::io_context& {
                std::lock_guard<std::mutex> g{mtx_con_iocs};
                auto& ret = *con_iocs_it++;
                if (con_iocs_it == con_iocs.end()) con_iocs_it = con_iocs.begin();
                return ret;
            };

        MQTT_NS::optional<server_no_tls> s;
        if (vm.count("tcp.port")) {
            s.emplace(
                accept_ioc,
                con_ioc_getter,
                b,
                vm["tcp.port"].as<std::uint16_t>()
            );
        }

#if defined(MQTT_USE_WS)
        MQTT_NS::optional<server_no_tls_ws> s_ws;
        if (vm.count("ws.port")) {
            s_ws.emplace(
                accept_ioc,
                con_ioc_getter,
                b,
                vm["ws.port"].as<std::uint16_t>()
            );
        }
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        MQTT_NS::optional<server_tls> s_tls;
        MQTT_NS::optional<as::steady_timer> s_lts_timer;

        if (vm.count("tls.port")) {
            s_tls.emplace(
                accept_ioc,
                con_ioc_getter,
                init_ctx(),
                b,
                vm["tls.port"].as<std::uint16_t>()
            );
            s_lts_timer.emplace(accept_ioc);
            load_ctx(s_tls.value(), s_lts_timer.value(), vm, "TLS");
        }
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)
        MQTT_NS::optional<server_tls_ws> s_tls_ws;
        MQTT_NS::optional<as::steady_timer> s_tls_ws_timer;

        if (vm.count("wss.port")) {
            s_tls_ws.emplace(
                accept_ioc,
                con_ioc_getter,
                init_ctx(),
                b,
                vm["wss.port"].as<std::uint16_t>()
            );
            s_tls_ws_timer.emplace(accept_ioc);
            load_ctx(s_tls_ws.value(), s_tls_ws_timer.value(), vm, "WSS");
        }
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)

        std::thread th_accept {
            [&accept_ioc] {
                accept_ioc.run();
                MQTT_LOG("mqtt_broker", trace) << "accept_ioc.run() finished";
            }
        };

        as::executor_work_guard<
            as::io_context::executor_type
        > guard_timer_ioc(timer_ioc.get_executor());

        std::thread th_timer {
            [&timer_ioc] {
                timer_ioc.run();
                MQTT_LOG("mqtt_broker", trace) << "timer_ioc.run() finished";
            }
        };
        std::vector<std::thread> ts;
        ts.reserve(num_of_iocs * threads_per_ioc);
        for (auto& con_ioc : con_iocs) {
            for (std::size_t i = 0; i != threads_per_ioc; ++i) {
                ts.emplace_back(
                    [&con_ioc] {
                        con_ioc.run();
                        MQTT_LOG("mqtt_broker", trace) << "con_ioc.run() finished";
                    }
                );
            }
        }

        th_accept.join();
        MQTT_LOG("mqtt_broker", trace) << "th_accept joined";

        for (auto& g : guard_con_iocs) g.reset();
        for (auto& t : ts) t.join();
        MQTT_LOG("mqtt_broker", trace) << "ts joined";

        guard_timer_ioc.reset();
        th_timer.join();
        MQTT_LOG("mqtt_broker", trace) << "th_timer joined";

    } catch(std::exception &e) {
        MQTT_LOG("mqtt_broker", error) << e.what();
    }
}

int main(int argc, char **argv) {
    try {
        boost::program_options::options_description desc;

        boost::program_options::options_description general_desc("General options");
        general_desc.add_options()
            ("help", "produce help message")
            (
                "cfg",
                boost::program_options::value<std::string>()->default_value("broker.conf"),
                "Load configuration file"
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
#if defined(MQTT_USE_LOG)
            (
                "verbose",
                boost::program_options::value<unsigned int>()->default_value(1),
                "set verbose level, possible values:\n 0 - Fatal\n 1 - Error\n 2 - Warning\n 3 - Info\n 4 - Debug\n 5 - Trace"
            )
#endif // defined(MQTT_USE_LOG)
            (
                "certificate",
                boost::program_options::value<std::string>(),
                "Certificate file for TLS connections"
            )
            (
                "private_key",
                boost::program_options::value<std::string>(),
                "Private key file for TLS connections"
            )
            (
                "certificate_reload_interval",
                boost::program_options::value<unsigned int>()->default_value(0),
                "Reload interval for the certificate and private key files (hours)\n 0 - Disabled"
            )
            (
                "auth_file",
                boost::program_options::value<std::string>()->default_value("auth.json"),
                "Authentication file"
            )
            ;

        boost::program_options::options_description notls_desc("TCP Server options");
        notls_desc.add_options()
            ("tcp.port", boost::program_options::value<std::uint16_t>(), "default port (TCP)")
        ;
        desc.add(general_desc).add(notls_desc);

#if defined(MQTT_USE_WS)
        boost::program_options::options_description ws_desc("TCP websocket Server options");
        ws_desc.add_options()
            ("ws.port", boost::program_options::value<std::uint16_t>(), "default port (TCP)")
        ;

        desc.add(ws_desc);
#endif // defined(MQTT_USE_WS)

#if defined(MQTT_USE_TLS)
        boost::program_options::options_description tls_desc("TLS Server options");
        tls_desc.add_options()
            ("tls.port", boost::program_options::value<std::uint16_t>(), "default port (TLS)")
        ;

        desc.add(tls_desc);
#endif // defined(MQTT_USE_TLS)

#if defined(MQTT_USE_WS) && defined(MQTT_USE_TLS)
        boost::program_options::options_description tlsws_desc("TLS Websocket Server options");
        tlsws_desc.add_options()
            ("wss.port", boost::program_options::value<std::uint16_t>(), "default port (TLS)")
        ;
        desc.add(tlsws_desc);
#endif // defined(MQTT_USE_TLS) && defined(MQTT_USE_WS)

        boost::program_options::variables_map vm;
        boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);

        std::string config_file = vm["cfg"].as<std::string>();
        if (!config_file.empty()) {
            std::ifstream input(vm["cfg"].as<std::string>());
            if (input.good()) {
                boost::program_options::store(boost::program_options::parse_config_file(input, desc), vm);
            } else
            {
                std::cerr << "Configuration file '" << config_file << "' not found,  broker doesn't use configuration file." << std::endl;
            }
        }

        boost::program_options::notify(vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 1;
        }

        std::cout << "Set options:" << std::endl;
        for (auto const& e : vm) {
            std::cout << boost::format("%-28s") % e.first.c_str() << " : ";
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

        run_broker(vm);
    } catch(std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
