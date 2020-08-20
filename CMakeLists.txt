CMAKE_MINIMUM_REQUIRED (VERSION 3.13.0)
PROJECT (mqtt_cpp_iface)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use GNU extensions
SET(CMAKE_CXX_EXTENSIONS OFF)
MESSAGE (STATUS "Setting minimum C++ standard to C++14")

OPTION(MQTT_BUILD_EXAMPLES "Enable building example applications" ON)
OPTION(MQTT_BUILD_TESTS "Enable building test applications" ON)
OPTION(MQTT_ALWAYS_SEND_REASON_CODE "Always send a reason code, even if the standard says it may be optionally omitted." ON)
OPTION(MQTT_USE_STATIC_BOOST "Statically link with boost libraries" OFF)
OPTION(MQTT_USE_STATIC_OPENSSL "Statically link with openssl libraries" OFF)
OPTION(MQTT_USE_TLS "Enable building TLS code" OFF)
OPTION(MQTT_USE_WS "Enable building WebSockets code" OFF)
OPTION(MQTT_USE_STR_CHECK "Enable UTF8 String check" ON)
OPTION(MQTT_STD_VARIANT "Use std::variant from C++17 instead of boost::variant" OFF)
OPTION(MQTT_STD_OPTIONAL "Use std::optional from C++17 instead of boost::optional" OFF)
OPTION(MQTT_STD_STRING_VIEW "Use std::string_view from C++17 instead of boost::string_view" OFF)
OPTION(MQTT_STD_ANY "Use std::any from C++17 instead of boost::any" OFF)
OPTION(MQTT_STD_SHARED_PTR_ARRAY "Use std::shared_ptr<char[]> from C++17 instead of boost::shared_ptr<char[]>" OFF)
OPTION(MQTT_DISABLE_LIBSTDCXX_TUPLE_ANY_WORKAROUND "std::tuple<std::any> workaround for libstdc++" OFF)

IF (POLICY CMP0074)
  CMAKE_POLICY(SET CMP0074 NEW)
ENDIF ()

IF (MQTT_USE_TLS)
    MESSAGE (STATUS "TLS enabled")
ELSE ()
    MESSAGE (STATUS "TLS disabled")
ENDIF ()

IF (MQTT_USE_WS)
    MESSAGE (STATUS "WebSocket enabled")
ELSE ()
    MESSAGE (STATUS "WebSocket disabled")
ENDIF ()

IF (MQTT_ALWAYS_SEND_REASON_CODE)
    MESSAGE (STATUS "Always send reason code enabled")
ELSE ()
    MESSAGE (STATUS "Always send reason code disabled")
ENDIF ()

IF (MQTT_USE_STR_CHECK)
    MESSAGE (STATUS "UTF8String check enabled")
ELSE ()
    MESSAGE (STATUS "UTF8String check disabled")
ENDIF ()

IF (MQTT_STD_VARIANT)
    MESSAGE (STATUS "Using std::variant instead of boost::variant. Enables C++17!!!")
ELSE ()
    MESSAGE (STATUS "Using boost::variant instead of std::variant")
ENDIF ()

IF (MQTT_STD_OPTIONAL)
    MESSAGE (STATUS "Using std::optional instead of boost::optional. Enables C++17!!!")
ELSE ()
    MESSAGE (STATUS "Using boost::optional instead of std::optional")
ENDIF ()

IF (MQTT_STD_STRING_VIEW)
    MESSAGE (STATUS "Using std::string_view instead of boost::string_view. Enables C++17!!!")
ELSE ()
    MESSAGE (STATUS "Using boost::string_view instead of std::string_view")
ENDIF ()

IF (MQTT_STD_ANY)
    MESSAGE (STATUS "Using std::any instead of boost::any. Enables C++17!!!")
ELSE ()
    MESSAGE (STATUS "Using boost::any instead of std::any")
ENDIF ()

IF (MQTT_STD_SHARED_PTR_ARRAY)
    MESSAGE (STATUS "Using std::shared_ptr<char []> instead of boost::shared_ptr<char []>. Enables C++17!!!")
ELSE ()
    MESSAGE (STATUS "Using boost::shared_ptr<char []> instead of std::shared_ptr<char []>")
ENDIF ()

IF (MQTT_DISABLE_LIBSTDCXX_TUPLE_ANY_WORKAROUND)
    MESSAGE (STATUS "std::tuple<std::any> workaround for libstdc++ disabled")
ELSE ()
    MESSAGE (STATUS "std::tuple<std::any> workaround for libstdc++ enabled")
ENDIF ()


IF (   MQTT_STD_VARIANT
    OR MQTT_STD_OPTIONAL
    OR MQTT_STD_ANY
    OR MQTT_STD_STRING_VIEW
    OR MQTT_STD_SHARED_PTR_ARRAY)
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE (STATUS "Setting minimum C++ standard to C++17!!!")
ENDIF ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET (CMAKE_CXX_FLAGS "/bigobj ${CMAKE_CXX_FLAGS}")
ENDIF ()

IF (MQTT_USE_STATIC_BOOST)
    MESSAGE (STATUS "Staticly linking with Boost")
    SET (Boost_USE_STATIC_LIBS TRUE)
ELSE ()
    MESSAGE (STATUS "Dynamically linking with Boost")
    SET (Boost_USE_STATIC_LIBS FALSE)
ENDIF ()

IF (MQTT_USE_STATIC_OPENSSL)
    MESSAGE (STATUS "Staticly linking with Openssl")
    SET (OPENSSL_USE_STATIC_LIBS TRUE)
ELSE ()
    MESSAGE (STATUS "Dynamically linking with Openssl")
    SET (OPENSSL_USE_STATIC_LIBS FALSE)
ENDIF ()

SET (Boost_USE_MULTITHREADED ON)
FIND_PACKAGE (Threads REQUIRED)

FIND_PACKAGE (Boost 1.67.0 REQUIRED COMPONENTS system date_time log)

IF (MQTT_USE_TLS)
    FIND_PACKAGE (OpenSSL REQUIRED)
    IF (MQTT_USE_STATIC_OPENSSL)
        FIND_PACKAGE (ZLIB REQUIRED)
    ENDIF ()
ENDIF ()

ADD_SUBDIRECTORY (include)

IF (MQTT_BUILD_TESTS)
    MESSAGE(STATUS "Tests enabled")
    ENABLE_TESTING ()
    ADD_SUBDIRECTORY (test)
ENDIF ()

IF (MQTT_BUILD_EXAMPLES)
    MESSAGE(STATUS "Examples enabled")
    ADD_SUBDIRECTORY (example)
ENDIF ()

# Doxygen
FIND_PACKAGE (Doxygen)
IF (DOXYGEN_FOUND)
    LIST (APPEND Doxyfile_CONTENT
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "FILE_PATTERNS      = *.hpp" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "OUTPUT_DIRECTORY   = doc" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "PROJECT_NAME       = \"mqtt_cpp\"" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "EXTRACT_ALL        = YES" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "RECURSIVE          = YES" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "PREDEFINED         = _DOXYGEN_" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "INPUT              = ${CMAKE_CURRENT_SOURCE_DIR}/include" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    )
    IF (DOXYGEN_DOT_FOUND)
        LIST (APPEND Doxyfile_CONTENT
            COMMAND ${CMAKE_COMMAND} -E echo "HAVE_DOT       = YES" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        )
    ENDIF ()
    ADD_CUSTOM_TARGET (
        doxygen
        ${Doxyfile_CONTENT}
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        VERBATIM
    )
ENDIF ()

include(cmake/createcmakeconfig.cmake)
