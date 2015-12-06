MQTT client for C++14 based on Boost.Asio
=========================================

Version 0.1.0 [![Build Status](https://travis-ci.org/redboltz/mqtt_client_cpp.svg?branch=master)](https://travis-ci.org/redboltz/mqtt_client_cpp) [![Build status](https://ci.appveyor.com/api/projects/status/21a267hd34s0kqu5?svg=true)](https://ci.appveyor.com/project/redboltz/mqtt_client_cpp/branch/master)

Overview
--------

mqtt_client_cpp is a header only library. It requires C++14 and the Boost Libraries.
Add mqtt_client_cpp/include to your include path. Then, include mqtt_client_cpp.hpp as follows:

```c++
#include <mqtt_client_cpp.hpp>
```

Example
-------

See ./example/no_tls.cpp and ./example/tls.cpp

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

You can create html documents using doxygen.

```
make doxygen
```

License
-------

mqtt_client_cpp is licensed under the Boost Software License, Version 1.0. See
the [`LICENSE_1_0.txt`](./LICENSE_1_0.txt) file for details.
