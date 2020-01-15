# Copyright Takatoshi Kondo 2015
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CMAKE_MINIMUM_REQUIRED (VERSION 3.8.2)

IF (POLICY CMP0074)
  CMAKE_POLICY(SET CMP0074 NEW)
ENDIF ()

OPTION(MQTT_TEST_1 "Build test part1" ON)
OPTION(MQTT_TEST_2 "Build test part2" ON)
OPTION(MQTT_TEST_3 "Build test part3" ON)
OPTION(MQTT_TEST_4 "Build test part4" ON)
OPTION(MQTT_TEST_5 "Build test part5" ON)
OPTION(MQTT_TEST_6 "Build test part6" ON)
OPTION(MQTT_TEST_7 "Build test part7" ON)

IF (MQTT_TEST_1)
    LIST (APPEND check_PROGRAMS
        connect.cpp
        underlying_timeout.cpp
    )
ENDIF ()

IF (MQTT_TEST_2)
    LIST (APPEND check_PROGRAMS
        length_check.cpp
        utf8string_validate.cpp
        packet_id.cpp
        remaining_length.cpp
        message.cpp
        property.cpp
    )
ENDIF ()

IF (MQTT_TEST_3)
    LIST (APPEND check_PROGRAMS
        sub.cpp
        pubsub.cpp
        pubsub_no_strand.cpp
        multi_sub.cpp
    )
ENDIF ()

IF (MQTT_TEST_4)
    LIST (APPEND check_PROGRAMS
        async_pubsub_1.cpp
        async_pubsub_2.cpp
    )
ENDIF ()

IF (MQTT_TEST_5)
    LIST (APPEND check_PROGRAMS
        resend.cpp
        resend_new_client.cpp
        resend_serialize.cpp
        resend_serialize_ptr_size.cpp
    )
ENDIF ()

IF (MQTT_TEST_6)
    LIST (APPEND check_PROGRAMS
        offline.cpp
        manual_publish.cpp
        retain.cpp
        will.cpp
    )
ENDIF ()

IF (MQTT_TEST_7)
    LIST (APPEND check_PROGRAMS
        as_buffer_sub.cpp
        as_buffer_pubsub.cpp
        as_buffer_async_pubsub_1.cpp
        as_buffer_async_pubsub_2.cpp
    )
ENDIF ()

FIND_PACKAGE (Boost 1.66.0 REQUIRED COMPONENTS unit_test_framework)

# Without this setting added, azure pipelines completely fails to find the boost libraries. No idea why.
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF ()

FOREACH (source_file ${check_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (${source_file_we} ${source_file})
    TARGET_COMPILE_DEFINITIONS (${source_file_we} PUBLIC $<IF:$<BOOL:${MQTT_USE_STATIC_BOOST}>,,BOOST_TEST_DYN_LINK>)
    TARGET_LINK_LIBRARIES (${source_file_we} mqtt_cpp_iface Boost::unit_test_framework)

    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        IF (MQTT_CODECOV)
            SET_PROPERTY (TARGET ${source_file_we}
                          APPEND_STRING PROPERTY COMPILE_FLAGS " -O0 -g --coverage -fno-inline")
            SET_PROPERTY (TARGET ${source_file_we}
                          APPEND_STRING PROPERTY LINK_FLAGS " --coverage")
        ENDIF ()
    ENDIF ()
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        IF (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
            STRING(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        ELSE ()
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
        ENDIF ()
    ENDIF ()

    ADD_TEST (${source_file_we} ${source_file_we})
ENDFOREACH ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.crt.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.key.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/cacert.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
ELSE ()
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.crt.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.key.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/cacert.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
ENDIF ()
