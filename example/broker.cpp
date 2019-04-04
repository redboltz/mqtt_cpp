#include "../test/test_broker.hpp"
#include "../test/test_server_no_tls.hpp"
#include "../test/test_server_tls.hpp"

int main() {
    boost::asio::io_service ios;
    test_broker b(ios);
    test_server_no_tls s(ios, b);
    test_server_tls st(ios, b);
    ios.run();
}
