#include "../test/test_server_no_tls.hpp"
#include "../test/test_broker.hpp"

int main() {
    boost::asio::io_context ioc;
    test_broker b(ioc);
    test_server_no_tls s(ioc, b);
    ioc.run();
}
