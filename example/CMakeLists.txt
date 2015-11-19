LIST (APPEND exec_PROGRAMS
    no_tls.cpp
)

IF (NOT MQTT_NO_TLS)
   LIST (APPEND exec_PROGRAMS
        tls.cpp
   )
ENDIF ()

FOREACH (source_file ${exec_PROGRAMS})
    GET_FILENAME_COMPONENT (source_file_we ${source_file} NAME_WE)
    ADD_EXECUTABLE (
        ${source_file_we}
        ${source_file}
    )
    IF (MQTT_NO_TLS)
        TARGET_LINK_LIBRARIES (${source_file_we}
            boost_system
            pthread
        )
    ELSE ()
        TARGET_LINK_LIBRARIES (${source_file_we}
            boost_system
            pthread
            ssl
            crypto
        )
    ENDIF ()
    IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        SET_PROPERTY (TARGET ${source_file_we}
                      APPEND_STRING PROPERTY COMPILE_FLAGS "-std=c++14 -Wall -Wextra -pthread -g -O0")
    ENDIF ()
ENDFOREACH ()
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
