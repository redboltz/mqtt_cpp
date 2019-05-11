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

install(FILES ${HDR_ROOT} DESTINATION ${ROOT_INCLUDE_TARGET})
install(FILES ${HDR_MQTT} DESTINATION ${ROOT_MQTT_TARGET})
