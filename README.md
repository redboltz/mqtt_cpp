# MQTT client/server for C++14 based on Boost.Asio

Version 13.2.2 [![Actions Status](https://github.com/redboltz/mqtt_cpp/workflows/CI/badge.svg)](https://github.com/redboltz/mqtt_cpp/actions)[![Build Status](https://dev.azure.com/redboltz/redboltz/_apis/build/status/redboltz.mqtt_cpp?branchName=master)](https://dev.azure.com/redboltz/redboltz/_build/latest?definitionId=6&branchName=master)[![codecov](https://codecov.io/gh/redboltz/mqtt_cpp/branch/master/graph/badge.svg)](https://codecov.io/gh/redboltz/mqtt_cpp)

**mqtt_cpp has been serious bugfix only status. The new project [async_mqtt](https://github.com/redboltz/async_mqtt) has been started based on mqtt_cpp's experience. New featureas would be added on async_mqtt.**

[MQTT v5 is supported](https://github.com/redboltz/mqtt_cpp/wiki/MQTT-v5) since version 4.0.0.

## Overview

mqtt_cpp is a header only library. It requires C++14 and the Boost Libraries 1.74.0 or later.

Add mqtt_cpp/include to your include path. Then, include `mqtt_client_cpp.hpp` and/or `mqtt_server_cpp.hpp` as follows:

For clients:
```c++
#include <mqtt_client_cpp.hpp>
#include <mqtt/setup_log.hpp> // if you want to use typical logging setup
```

For servers:
```c++
#include <mqtt_server_cpp.hpp>
#include <mqtt/setup_log.hpp> // if you want to use typical logging setup
```

You can compile your code as follows:

```
g++ -std=c++14 -Ipath_to_mqtt_cpp/include your_program.cpp -pthread additional_options
```

mqtt_cpp uses std::variant or boost::variant internally.
If you define the preprocessor macro MQTT_STD_VARIANT then std::variant is chosen, otherwise boost::variant is chosen.

If you use boost::variant, you need to include [mqtt/config.hpp](https://github.com/redboltz/mqtt_cpp/blob/master/include/mqtt/config.hpp) prior to the boost headers inclusion becausse it defines boost configuration macros.
When you use [mqtt_client_cpp.hpp](https://github.com/redboltz/mqtt_cpp/blob/master/include/mqtt_client_cpp.hpp) and/or [mqtt_server_cpp.hpp](https://github.com/redboltz/mqtt_cpp/blob/master/include/mqtt_server_cpp.hpp), they include [mqtt/config.hpp](https://github.com/redboltz/mqtt_cpp/blob/master/include/mqtt/config.hpp) internally.

### Experimental feature

In `include/mqtt/broker`, there are header files to implement mqtt broker.
It is an experimental feature. It would frequently updated. Even if the broker interface would be changed, major version won't be updated.


**additional_options**

|what you want|required option|
|---|---|
|TLS support|`-DMQTT_USE_TLS -pthread -lssl -lcrypto`|
|Logging support|`-DMQTT_USE_LOG -DBOOST_LOG_DYN_LINK -lboost_log -lboost_filesystem -lboost_thread`|
|WebSocket support|`-DMQTT_USE_WS`|

You can see more detail at https://github.com/redboltz/mqtt_cpp/wiki/Config


## Example

* NO TLS
  * Client
    * TCP
      * [example/no_tls_client.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_client.cpp)
    * WebSocket
      * [example/no_tls_ws_client.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_ws_client.cpp)
  * Server
    * TCP
      * [example/no_tls_server.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_server.cpp)
    * WebSocket
      * [example/no_tls_ws_server.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_ws_server.cpp)
  * Client and Server
    * TCP
      * [example/no_tls_both.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_both.cpp)
    * WebSocket
      * [example/no_tls_ws_both.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/no_tls_ws_both.cpp)
* TLS
  * Client
    * TCP
      * [example/tls_client.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_client.cpp)
    * WebSocket
      * [example/tls_ws_client.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_ws_client.cpp)
  * Server
    * TCP
      * [example/tls_server.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_server.cpp)
    * WebSocket
      * [example/tls_ws_server.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_ws_server.cpp)
  * Client and Server
    * TCP
      * [example/tls_both.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_both.cpp)
    * WebSocket
      * [example/tls_ws_both.cpp](https://github.com/redboltz/mqtt_cpp/blob/master/example/tls_ws_both.cpp)

## Usage in cmake project

Add following lines to your `CMakeLists.txt`

```
set(LIBNAME "your_project_name")
add_library(${LIBNAME} <your dependencies here>)

find_package(mqtt_cpp_iface REQUIRED)
target_link_libraries(${LIBNAME} LINK_PUBLIC mqtt_cpp_iface::mqtt_cpp_iface)
```

This uses the `mqtt_cpp_iface::mqtt_cpp_iface` target and includes all necessary link dependencies.

## Test

You can build tests and examples as follows:


At mqtt_cpp directory

```
mkdir build
cd build
cmake ..
make
make test
```

## Documents
https://github.com/redboltz/mqtt_cpp/wiki

You can create html documents using doxygen.

```
make doxygen
```

## License

mqtt_cpp is licensed under the Boost Software License, Version 1.0. See
the [`LICENSE_1_0.txt`](./LICENSE_1_0.txt) file for details.
