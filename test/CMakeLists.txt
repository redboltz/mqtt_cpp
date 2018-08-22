# Copyright Takatoshi Kondo 2015
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

CMAKE_MINIMUM_REQUIRED (VERSION 2.8.6)

LIST (APPEND check_PROGRAMS
     connect.cpp
     pubsub.cpp
     async_pubsub_1.cpp
     async_pubsub_2.cpp
     pubsub_no_strand.cpp
     remaining_length.cpp
     multi_sub.cpp
     resend.cpp
     resend_serialize.cpp
     resend_serialize_ptr_size.cpp
     offline.cpp
     manual_publish.cpp
     retain.cpp
     will.cpp
     length_check.cpp
     sub.cpp
     as_buffer_sub.cpp
     as_buffer_pubsub.cpp
     as_buffer_async_pubsub_1.cpp
     as_buffer_async_pubsub_2.cpp
     utf8string_validate.cpp
)

LIST (APPEND MQTT_LINK_LIBRARIES
    ${Boost_TEST_EXEC_MONITOR_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${CMAKE_THREAD_LIBS_INIT}
)
IF (NOT MQTT_NO_TLS)
    LIST (APPEND MQTT_LINK_LIBRARIES
        ${OPENSSL_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )
ENDIF ()

LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})

FOREACH (source_file ${check_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (
        ${source_file_we}
        ${source_file}
    )
    ADD_TEST (${source_file_we} ${source_file_we})
    TARGET_LINK_LIBRARIES (${source_file_we}
        ${MQTT_LINK_LIBRARIES}
    )
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET_PROPERTY (TARGET ${source_file_we}
                      APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++14 -pthread")
    ENDIF ()
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        IF (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
            STRING(REGEX REPLACE "/W[0-4]" "/W3 /WX" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        ELSE ()
            SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /WX")
        ENDIF ()
    ENDIF ()
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
