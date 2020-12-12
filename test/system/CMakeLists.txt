# Copyright Takatoshi Kondo 2015
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CMAKE_MINIMUM_REQUIRED (VERSION 3.8.2)

IF (POLICY CMP0074)
  CMAKE_POLICY(SET CMP0074 NEW)
ENDIF ()

IF (MQTT_TEST_2)
    LIST (APPEND check_PROGRAMS
        st_connect.cpp
        st_underlying_timeout.cpp
        st_as_buffer_sub.cpp
        st_topic_alias_recv.cpp
        st_as_buffer_pubsub.cpp
        st_shared_sub.cpp
    )
ENDIF ()

IF (MQTT_TEST_3)
    LIST (APPEND check_PROGRAMS
        st_pubsub_1.cpp
        st_sub.cpp
        st_pubsub_2.cpp
        st_pubsub_no_strand.cpp
        st_as_buffer_async_pubsub_1.cpp
        st_multi_sub.cpp
        st_as_buffer_async_pubsub_2.cpp
    )
ENDIF ()

IF (MQTT_TEST_4)
    LIST (APPEND check_PROGRAMS
        st_async_pubsub_1.cpp
    )
ENDIF ()

IF (MQTT_TEST_5)
    LIST (APPEND check_PROGRAMS
        st_resend.cpp
        st_retain_1.cpp
        st_resend_new_client.cpp
        st_retain_2.cpp
        st_will.cpp
    )
ENDIF ()

IF (MQTT_TEST_6)
    LIST (APPEND check_PROGRAMS
        st_offline.cpp
        st_manual_publish.cpp
    )
ENDIF ()

IF (MQTT_TEST_7)
    LIST (APPEND check_PROGRAMS
        st_async_pubsub_2.cpp
    )
ENDIF ()

FIND_PACKAGE (Boost 1.67.0 REQUIRED COMPONENTS unit_test_framework)

# Without this setting added, azure pipelines completely fails to find the boost libraries. No idea why.
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF ()

FOREACH (source_file ${check_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (${source_file_we} ${source_file})
    TARGET_COMPILE_DEFINITIONS (${source_file_we} PUBLIC $<IF:$<BOOL:${MQTT_USE_STATIC_BOOST}>,,BOOST_TEST_DYN_LINK>)
    TARGET_LINK_LIBRARIES (
        ${source_file_we} mqtt_cpp_iface Boost::unit_test_framework
    )
    IF (MQTT_USE_LOG)
        TARGET_COMPILE_DEFINITIONS (${source_file_we} PUBLIC $<IF:$<BOOL:${MQTT_USE_STATIC_BOOST}>,,BOOST_LOG_DYN_LINK>)
        TARGET_LINK_LIBRARIES (
            ${source_file_we} Boost::log
        )
    ENDIF ()
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

    # Running test with arguments
    # CTEST_ARGS="--log_level=all" ctest -V
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        ADD_TEST (NAME ${source_file_we} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${source_file_we})
    ELSE ()
        ADD_TEST (NAME ${source_file_we} COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../args_provider.sh ${CMAKE_CURRENT_BINARY_DIR}/${source_file_we})
    ENDIF ()
    set_tests_properties(${source_file_we} PROPERTIES TIMEOUT 15)
ENDFOREACH ()

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/server.crt.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/server.key.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/cacert.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
ELSE ()
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/server.crt.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/server.key.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
   FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../certs/cacert.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
ENDIF ()
