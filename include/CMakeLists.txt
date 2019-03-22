
file(GLOB HDR_ROOT "*.hpp")
file(GLOB HDR_MQTT "mqtt/*.hpp")

add_library(mqtt_cpp_iface INTERFACE)

target_include_directories(mqtt_cpp_iface INTERFACE .)

install(FILES ${HDR_ROOT} DESTINATION include)
install(FILES ${HDR_MQTT} DESTINATION include/mqtt)

