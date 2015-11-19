# Copyright Takatoshi Kondo 2015
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CMAKE_MINIMUM_REQUIRED (VERSION 2.8.6)

PROJECT (mqtt_client_cpp_test)

LIST (APPEND check_PROGRAMS
     test_main.cpp
     connect.cpp
     pubsub.cpp
     multi_sub.cpp
     resend.cpp
     manual_publish.cpp
     retain.cpp
     will.cpp
)

ADD_EXECUTABLE (${PROJECT_NAME} ${check_PROGRAMS})
TARGET_LINK_LIBRARIES (${PROJECT_NAME}
    boost_test_exec_monitor
    boost_system
    pthread
    ssl
    crypto
)

ADD_TEST (${PROJECT_NAME} ${PROJECT_NAME})

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    SET_PROPERTY (TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++14 -Wall -Wextra -pthread -g -O0")
ENDIF ()

FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
