FILE(GLOB HDR_ROOT "*.hpp")
FILE(GLOB HDR_MQTT "mqtt/*.hpp")

SET(ROOT_INCLUDE_TARGET include)
SET(ROOT_MQTT_TARGET include/mqtt)

ADD_LIBRARY(mqtt_cpp_iface INTERFACE)

TARGET_LINK_LIBRARIES(mqtt_cpp_iface INTERFACE Threads::Threads Boost::system Boost::date_time)

IF(MQTT_USE_STATIC_LINKING)
    # Unfortunately, cmake doesn't automatically detect that statically linked openssl
    # requires at least two more libs to be linked. "dl" is for dynamic library linking
    # and zlib is for basic compression functionality.
    # There is currently no way to specify statically linked zlib.
    TARGET_LINK_LIBRARIES(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_USE_TLS}>:${CMAKE_DL_LIBS} ZLIB::ZLIB>)
ENDIF()

TARGET_LINK_LIBRARIES(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_USE_TLS}>:OpenSSL::SSL>)

TARGET_INCLUDE_DIRECTORIES(mqtt_cpp_iface
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${ROOT_INCLUDE_TARGET}>
    $<INSTALL_INTERFACE:${ROOT_MQTT_TARGET}>
)

TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_USE_TLS}>:MQTT_USE_TLS>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_USE_WS}>:MQTT_USE_WS>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_USE_STR_CHECK}>:MQTT_USE_STR_CHECK>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE MQTT_ALWAYS_SEND_REASON_CODE=$<BOOL:${MQTT_ALWAYS_SEND_REASON_CODE}>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_STD_VARIANT}>:MQTT_STD_VARIANT>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_STD_OPTIONAL}>:MQTT_STD_OPTIONAL>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_STD_ANY}>:MQTT_STD_ANY>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_STD_STRING_VIEW}>:MQTT_STD_STRING_VIEW>)
TARGET_COMPILE_DEFINITIONS(mqtt_cpp_iface INTERFACE $<$<BOOL:${MQTT_STD_SHARED_PTR_ARRAY}>:MQTT_STD_SHARED_PTR_ARRAY>)

# You might wonder why we don't simply add the list of header files to the check_deps
# executable directly, and let cmake figure everything out on it's own.
# The reason we can't do that is because cmake has built in rules that can't be disabled
# or overridden that prevent it from ever treating a file that ends in .h as a source file.
# So the work around for this problem is that we copy the header files to a different folder,
# one that's owned by CMake (the binary dir), and compile the header files there to perform
# the dependency checking.
SET (HDR_DEPS_CHK_FOLDER "${CMAKE_BINARY_DIR}/check_deps_headers")
FILE (MAKE_DIRECTORY "${HDR_DEPS_CHK_FOLDER}")
LIST (APPEND CHK_MQTT)
FILE (GLOB_RECURSE ALL_HPP "mqtt/*.hpp")
FOREACH (HDR ${ALL_HPP})
    GET_FILENAME_COMPONENT (HDR_NAME ${HDR} NAME)
    # Of course, the FILE macro always treats the destination name as if it were a folder name
    # So we have to first copy the header file, and THEN rename the header file to have an
    # extension that cmake will treat as a source file.
    FILE (COPY ${HDR} DESTINATION "${HDR_DEPS_CHK_FOLDER}")
    FILE (RENAME "${HDR_DEPS_CHK_FOLDER}/${HDR_NAME}" "${HDR_DEPS_CHK_FOLDER}/${HDR_NAME}.cpp")
    LIST (APPEND CHK_MQTT "${HDR_DEPS_CHK_FOLDER}/${HDR_NAME}.cpp")
ENDFOREACH ()
# The EXCLUDE_FROM_ALL property prevents check_deps from being included in the default target
ADD_LIBRARY (check_deps EXCLUDE_FROM_ALL ${CHK_MQTT})
# We don't need to actually run the whole compiler. We're just checking that all the includes are valid
# So here we ask for only the preprocessor to run.
# We also don't mind that there might be unused constant variables when doing deps checking.
TARGET_COMPILE_OPTIONS(check_deps PUBLIC -Wno-unused-const-variable $<$<CXX_COMPILER_ID:MSVC>:/P,--preprocess>)
TARGET_LINK_LIBRARIES (check_deps PUBLIC mqtt_cpp_iface)

INSTALL(FILES ${HDR_ROOT} DESTINATION ${ROOT_INCLUDE_TARGET})
INSTALL(FILES ${HDR_MQTT} DESTINATION ${ROOT_MQTT_TARGET})
