LIST (APPEND exec_PROGRAMS
    no_tls_client.cpp
    no_tls_server.cpp
    no_tls_both.cpp
)

IF (NOT MQTT_NO_TLS)
    LIST (APPEND exec_PROGRAMS
        tls_client.cpp
        tls_server.cpp
        tls_both.cpp
    )
ENDIF ()

IF (MQTT_USE_WS)
    LIST (APPEND exec_PROGRAMS
        no_tls_ws_client.cpp
        no_tls_ws_server.cpp
        no_tls_ws_both.cpp
    )
    IF (NOT MQTT_NO_TLS)
        LIST (APPEND exec_PROGRAMS
            tls_ws_client.cpp
            tls_ws_server.cpp
            tls_ws_both.cpp
        )
    ENDIF ()
ENDIF ()


FOREACH (source_file ${exec_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (
        ${source_file_we}
        ${source_file}
    )
    LIST (APPEND MQTT_LINK_LIBRARIES
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
    TARGET_LINK_LIBRARIES (${source_file_we}
        ${MQTT_LINK_LIBRARIES}
    )
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET_PROPERTY (TARGET ${source_file_we}
                      APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++14 -Wall -Wextra -pthread -g -O0")
    ENDIF ()
ENDFOREACH ()
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.crt.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/server.key.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../test/certs/cacert.pem DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
