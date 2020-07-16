#include "../test/test_server_no_tls.hpp"
#include "../test/test_broker.hpp"
#include <mqtt/setup_log.hpp>

int main() {
    MQTT_NS::setup_log();
    boost::asio::io_context ioc;
    test_broker b(ioc);
    test_server_no_tls s(ioc, b);
    ioc.run();
}
