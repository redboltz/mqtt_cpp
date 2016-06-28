MQTT client for C++14 based on Boost.Asio
=========================================

Version 1.0.2 [![Build Status](https://travis-ci.org/redboltz/mqtt_client_cpp.svg?branch=master)](https://travis-ci.org/redboltz/mqtt_client_cpp) [![Build status](https://ci.appveyor.com/api/projects/status/21a267hd34s0kqu5/branch/master?svg=true)](https://ci.appveyor.com/project/redboltz/mqtt-client-cpp/branch/master)

Overview
--------

mqtt_client_cpp is a header only library. It requires C++14 and the Boost Libraries 1.57.0 or later (See [#33](https://github.com/redboltz/mqtt_client_cpp/issues/33)). The only feature that mqtt_client_cpp uses C++14 is binary literals. e.g.) 0b00001111

You may be able to use mqtt_client_cpp on C++11 with binary literal extension environment such as g++ and clang++.

Add mqtt_client_cpp/include to your include path. Then, include mqtt_client_cpp.hpp as follows:

```c++
#include <mqtt_client_cpp.hpp>
```

You can compile your program as follows:

```
g++ -std=c++14 -Ipath_to_mqtt_client_cpp/include -DMQTT_NO_TLS no_tls.cpp -lboost_system -lpthread
```

```
g++ -std=c++14 -Ipath_to_mqtt_client_cpp/include tls.cpp -lboost_system -lssl -lcrypto -lpthread
```

Example
-------

See [example/no_tls.cpp](https://github.com/redboltz/mqtt_client_cpp/blob/master/example/no_tls.cpp) and [example/tls.cpp](https://github.com/redboltz/mqtt_client_cpp/blob/master/example/tls.cpp)

Test
----

You can build tests and examples as follows:


At mqtt_client_cpp directory

```
mkdir build
cd build
cmake ..
make
make test
```

In order to build tests, you need to prepare the Boost Libraries 1.59.0.

Documents
---------
http://redboltz.github.io/contents/mqtt/index.html

You can create html documents using doxygen.

```
make doxygen
```

License
-------

mqtt_client_cpp is licensed under the Boost Software License, Version 1.0. See
the [`LICENSE_1_0.txt`](./LICENSE_1_0.txt) file for details.
