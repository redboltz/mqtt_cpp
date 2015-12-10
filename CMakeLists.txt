CMAKE_MINIMUM_REQUIRED (VERSION 2.8.6)
PROJECT (mqtt_client_cpp)

IF (MQTT_NO_TLS)
    SET (CMAKE_CXX_FLAGS "-DMQTT_NO_TLS ${CMAKE_CXX_FLAGS}")
ELSE ()
    FIND_PACKAGE (OpenSSL)
ENDIF ()

SET (Boost_USE_STATIC_LIBS        ON) # only find static libs
SET (Boost_USE_MULTITHREADED      ON)
# SET (Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE (Boost 1.59.0 COMPONENTS chrono timer test_exec_monitor system)
FIND_PACKAGE (Threads)

INCLUDE_DIRECTORIES (
    ${Boost_INCLUDE_DIR}
    include
    ${OpenSSL_INCLUDE_DIR}
    ${MQTT_OPENSSL_INCLUDE_DIR}
)

ENABLE_TESTING ()
ADD_SUBDIRECTORY (test)
ADD_SUBDIRECTORY (example)

# Doxygen
FIND_PACKAGE (Doxygen)
IF (DOXYGEN_FOUND)
    LIST (APPEND Doxyfile_CONTENT
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "FILE_PATTERNS      = *.hpp" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "OUTPUT_DIRECTORY   = doc" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        COMMAND ${CMAKE_COMMAND} -E echo "PROJECT_NAME       = \"mqtt_client_cpp\"" >> ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
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
