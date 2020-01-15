LIST (APPEND exec_PROGRAMS
    no_tls_client.cpp
    no_tls_server.cpp
    no_tls_both.cpp
    v5_no_tls_client.cpp
    v5_no_tls_server.cpp
    v5_no_tls_both.cpp
    v5_no_tls_prop.cpp
    redirect.cpp
    broker.cpp
)

IF (MQTT_USE_TLS)
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
ENDIF ()

IF (MQTT_USE_WS AND MQTT_USE_TLS)
    LIST (APPEND exec_PROGRAMS
        tls_ws_client.cpp
        tls_ws_server.cpp
        tls_ws_both.cpp
    )
ENDIF ()

# Without this setting added, azure pipelines completely fails to find the boost libraries. No idea why.
IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
ENDIF ()

FOREACH (source_file ${exec_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (${source_file_we} ${source_file})
    TARGET_LINK_LIBRARIES (${source_file_we} mqtt_cpp_iface)
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
