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
    LIST (APPEND MQTT_LINK_LIBRARIES
        ${Boost_SYSTEM_LIBRARY}
        ${CMAKE_THREAD_LIBS_INIT}
    )
    IF (NOT MQTT_NO_TLS)
        LIST (APPEND MQTT_LINK_LIBRARIES
            ${OPENSSL_LIBRARIES}
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
FILE(COPY ${CMAKE_CURRENT_SOURCE_DIR}/../mosquitto.org.crt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
