file(GLOB HDR_ROOT "*.hpp")
file(GLOB HDR_MQTT "mqtt/*.hpp")

add_library(mqtt_cpp_iface INTERFACE)

set(ROOT_INCLUDE_TARGET include)
set(ROOT_MQTT_TARGET include/mqtt)

target_include_directories(mqtt_cpp_iface
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${ROOT_INCLUDE_TARGET}>
    $<INSTALL_INTERFACE:${ROOT_MQTT_TARGET}>
)

IF (MQTT_NO_TLS)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_NO_TLS)
ENDIF ()

IF (MQTT_USE_WS)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_USE_WS)
ENDIF ()

IF (MQTT_USE_STR_CHECK)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_USE_STR_CHECK)
ENDIF ()

IF (MQTT_STD_VARIANT)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_STD_VARIANT)
ENDIF ()

IF (MQTT_STD_OPTIONAL)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_STD_OPTIONAL)
ENDIF ()

IF (MQTT_STD_STRING_VIEW)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_STD_STRING_VIEW)
ENDIF ()

IF (MQTT_STD_ANY)
    target_compile_definitions(mqtt_cpp_iface INTERFACE MQTT_STD_ANY)
ENDIF ()

target_link_libraries(mqtt_cpp_iface INTERFACE Boost::system Threads::Threads)

list(APPEND CHK_INCLUDE_DIRS -I${Boost_INCLUDE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR})
IF (NOT MQTT_NO_TLS)
    list(APPEND CHK_INCLUDE_DIRS -I${OPENSSL_INCLUDE_DIR})
    IF (MQTT_OPENSSL_INCLUDE_DIR)
        list(APPEND CHK_INCLUDE_DIRS -I${MQTT_OPENSSL_INCLUDE_DIR})
    ENDIF ()
ENDIF ()

#str to list
string(REPLACE " " ";" CHK_CXX_FLAGS ${CMAKE_CXX_FLAGS})
IF ($ENV{CXXFLAGS})
    string(REPLACE " " ";" CHK_ENV_CXXFLAGS $ENV{CXXFLAGS})
ENDIF ()

FOREACH (HDR ${HDR_MQTT})
    get_filename_component(HDR_WE ${HDR} NAME_WE)
    set(CHK_TARGET "${HDR_WE}.chk")
    add_custom_target(
        ${CHK_TARGET}
        COMMAND ${CMAKE_CXX_COMPILER} -c ${CHK_ENV_CXX_FLAGS} ${CHK_CXX_FLAGS} ${CHK_INCLUDE_DIRS} ${HDR}
        VERBATIM
    )
   add_dependencies(check_deps ${CHK_TARGET})
ENDFOREACH ()

install(FILES ${HDR_ROOT} DESTINATION ${ROOT_INCLUDE_TARGET})
install(FILES ${HDR_MQTT} DESTINATION ${ROOT_MQTT_TARGET})
