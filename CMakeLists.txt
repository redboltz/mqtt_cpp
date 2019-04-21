CMAKE_MINIMUM_REQUIRED (VERSION 3.8.2)
PROJECT (mqtt_cpp)

OPTION(BUILD_EXAMPLES "Enable building example applications" ON)
OPTION(BUILD_TESTS "Enable building test applications" ON)
OPTION(MQTT_USE_STATIC_BOOST "Statically link with boost libraries" OFF)
OPTION(MQTT_NO_TLS "Disable building TLS code" ON)
OPTION(MQTT_USE_WS "Enable building WebSockets code" OFF)
OPTION(MQTT_USE_STR_CHECK "Enable UTF8 String check" ON)
OPTION(MQTT_STD_VARIANT "Use std::variant from C++17 instead of boost::variant" OFF)
OPTION(MQTT_STD_OPTIONAL "Use std::optional from C++17 instead of boost::optional" OFF)
OPTION(MQTT_STD_STRING_VIEW "Use std::string_view from C++17 instead of boost::string_view" OFF)
OPTION(MQTT_STD_ANY "Use std::any from C++17 instead of boost::any" OFF)

IF ((CMAKE_VERSION VERSION_GREATER 3.1) OR
    (CMAKE_VERSION VERSION_EQUAL 3.1))
    CMAKE_POLICY(SET CMP0054 NEW)
ENDIF ()

IF (MQTT_USE_STATIC_BOOST)
    SET (Boost_USE_STATIC_LIBS ON) # only find static libs
ELSE ()
    SET (Boost_USE_STATIC_LIBS OFF) # only find shared libs
ENDIF ()

IF (MQTT_NO_TLS)
    MESSAGE (STATUS "TLS disabled")
    SET (CMAKE_CXX_FLAGS "-DMQTT_NO_TLS ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "TLS enabled")
    FIND_PACKAGE (OpenSSL)
ENDIF ()

IF (MQTT_USE_WS)
    MESSAGE (STATUS "WebSocket enabled")
    SET (CMAKE_CXX_FLAGS "-DMQTT_USE_WS ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "WebSocket disabled")
ENDIF ()

IF (MQTT_USE_STR_CHECK)
    MESSAGE (STATUS "UTF8String check enabled")
    SET (CMAKE_CXX_FLAGS "-DMQTT_USE_STR_CHECK ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "UTF8String check disabled")
ENDIF ()

IF (MQTT_STD_VARIANT)
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE (STATUS "Using std::variant instead of boost::variant. Enables C++17!!!")
    SET (CMAKE_CXX_FLAGS "-DMQTT_STD_VARIANT ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "Using boost::variant instead of std::variant")
ENDIF ()

IF (MQTT_STD_OPTIONAL)
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE (STATUS "Using std::optional instead of boost::optional. Enables C++17!!!")
    SET (CMAKE_CXX_FLAGS "-DMQTT_STD_OPTIONAL ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "Using boost::optional instead of std::optional")
ENDIF ()

IF (MQTT_STD_STRING_VIEW)
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE (STATUS "Using std::string_view instead of boost::string_view. Enables C++17!!!")
    SET (CMAKE_CXX_FLAGS "-DMQTT_STD_STRING_VIEW ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "Using boost::string_view instead of std::string_view")
ENDIF ()

IF (MQTT_STD_ANY_VIEW)
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE (STATUS "Using std::any instead of boost::any. Enables C++17!!!")
    SET (CMAKE_CXX_FLAGS "-DMQTT_STD_ANY ${CMAKE_CXX_FLAGS}")
ELSE ()
    MESSAGE (STATUS "Using boost::any instead of std::any")
ENDIF ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    SET (CMAKE_CXX_FLAGS "/bigobj ${CMAKE_CXX_FLAGS}")
ENDIF ()

SET (Boost_USE_MULTITHREADED      ON)
FIND_PACKAGE (Boost 1.59.0 REQUIRED COMPONENTS system)
FIND_PACKAGE (Threads)

IF(BUILD_TESTS)
    FIND_PACKAGE (Boost 1.59.0 REQUIRED COMPONENTS chrono timer unit_test_framework)
ENDIF()

IF (NOT MQTT_BEAST_INCLUDE_DIR)
    IF (${Boost_VERSION} LESS 106600)
        SET (MQTT_BEAST_INCLUDE_DIR "external/beast/include")
    ENDIF ()
ENDIF ()

INCLUDE_DIRECTORIES (
    ${Boost_INCLUDE_DIR}
    ${MQTT_BEAST_INCLUDE_DIR}
    include
)

IF (NOT MQTT_NO_TLS)
    INCLUDE_DIRECTORIES (
        ${OPENSSL_INCLUDE_DIR}
        ${MQTT_OPENSSL_INCLUDE_DIR}
    )
ENDIF ()

ENABLE_TESTING ()

IF (BUILD_TESTS)
    MESSAGE(STATUS "Tests enabled")
    ADD_SUBDIRECTORY (test)
ENDIF ()

IF (BUILD_EXAMPLES)
    MESSAGE(STATUS "Examples enabled")
    ADD_SUBDIRECTORY (example)
ENDIF ()

ADD_SUBDIRECTORY (include)

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
