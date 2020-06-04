## Installation and preparation of the Config.cmake file.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

configure_file(${CMAKE_CURRENT_LIST_DIR}/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    @ONLY
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
  DESTINATION
    ${INSTALL_CONFIGDIR}
)

set(LIB_INSTALL_PATH lib)
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets DESTINATION ${LIB_INSTALL_PATH})
install(EXPORT ${PROJECT_NAME}Targets
	FILE ${PROJECT_NAME}Targets.cmake
	NAMESPACE ${PROJECT_NAME}::
	DESTINATION ${INSTALL_CONFIGDIR})

