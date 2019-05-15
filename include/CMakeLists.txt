find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

file(GLOB HDR_ROOT "*.hpp")
file(GLOB HDR_MQTT "mqtt/*.hpp")

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE Boost::system Threads::Threads)

IF(!MQTT_NO_TLS)
    target_link_libraries(${PROJECT_NAME} INTERFACE Boost::ssl)
ENDIF()

set(ROOT_INCLUDE_TARGET include)
set(ROOT_MQTT_TARGET include/mqtt)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${ROOT_INCLUDE_TARGET}>
    $<INSTALL_INTERFACE:${ROOT_MQTT_TARGET}>
)

install(FILES ${HDR_ROOT} DESTINATION ${ROOT_INCLUDE_TARGET})
install(FILES ${HDR_MQTT} DESTINATION ${ROOT_MQTT_TARGET})

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
  EXPORT ${PROJECT_NAME}-targets
  DESTINATION include
)

install(EXPORT ${PROJECT_NAME}-targets
  FILE
    ${PROJECT_NAME}Targets.cmake
  NAMESPACE
    ${PROJECT_NAME}::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/../cmake/${PROJECT_NAME}Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

target_compile_definitions(${PROJECT_NAME} INTERFACE
    -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
    -DBOOST_MPL_LIMIT_LIST_SIZE=40
    )

IF (MQTT_NO_TLS)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_NO_TLS)
ENDIF ()

IF (MQTT_USE_WS)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_USE_WS)
ENDIF ()

IF (MQTT_USE_STR_CHECK)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_USE_STR_CHECK)
ENDIF ()

IF (MQTT_STD_VARIANT)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_STD_VARIANT)
ENDIF ()

IF (MQTT_STD_OPTIONAL)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_STD_OPTIONAL)
ENDIF ()

IF (MQTT_STD_STRING_VIEW)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_STD_STRING_VIEW)
ENDIF ()

IF (MQTT_STD_ANY)
    target_compile_definitions(${PROJECT_NAME} INTERFACE MQTT_STD_ANY)
ENDIF ()
