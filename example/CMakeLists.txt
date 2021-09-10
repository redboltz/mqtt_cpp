LIST (APPEND exec_PROGRAMS
    no_tls_client.cpp
    no_tls_async_client.cpp
    no_tls_server.cpp
    no_tls_both.cpp
    v5_no_tls_client.cpp
    v5_no_tls_server.cpp
    v5_no_tls_both.cpp
    v5_no_tls_prop.cpp
    long_lived_client.cpp
    redirect.cpp
    broker.cpp
)

FIND_PACKAGE (Boost 1.67.0 REQUIRED COMPONENTS program_options)

IF (MQTT_USE_TLS)
    LIST (APPEND exec_PROGRAMS
        tls_client.cpp
        tls_server.cpp
        tls_both.cpp
        tls_both_client_cert.cpp
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

IF (MQTT_USE_LOG)
    LIST (APPEND exec_PROGRAMS
        logging.cpp
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

    IF (WIN32 AND MQTT_USE_STATIC_OPENSSL)
        TARGET_LINK_LIBRARIES (${source_file_we} Crypt32)
    ENDIF ()

    IF (MQTT_USE_LOG)
        TARGET_COMPILE_DEFINITIONS (${source_file_we} PUBLIC $<IF:$<BOOL:${MQTT_USE_STATIC_BOOST}>,,BOOST_LOG_DYN_LINK>)
        TARGET_LINK_LIBRARIES (${source_file_we} Boost::log)
    ENDIF ()
    TARGET_COMPILE_DEFINITIONS (${source_file_we} PUBLIC $<IF:$<BOOL:${MQTT_USE_STATIC_BOOST}>,,BOOST_PROGRAM_OPTIONS_DYN_LINK>)
    TARGET_LINK_LIBRARIES (${source_file_we} Boost::program_options)
ENDFOREACH ()

FILE(COPY broker.conf DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/mosquitto.org.crt DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/server.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/server.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/client.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/client.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )
FILE(COPY ../test/certs/cacert.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}" )

IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
   FILE(COPY broker.conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Release)
   FILE(COPY ../test/certs/mosquitto.org.crt DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
   FILE(COPY ../test/certs/server.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
   FILE(COPY ../test/certs/server.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
   FILE(COPY ../test/certs/client.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
   FILE(COPY ../test/certs/client.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")
   FILE(COPY ../test/certs/cacert.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Release")

   FILE(COPY broker.conf DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Debug)
   FILE(COPY ../test/certs/mosquitto.org.crt DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
   FILE(COPY ../test/certs/server.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
   FILE(COPY ../test/certs/server.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
   FILE(COPY ../test/certs/client.crt.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
   FILE(COPY ../test/certs/client.key.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")
   FILE(COPY ../test/certs/cacert.pem DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/Debug")

 ENDIF ()
