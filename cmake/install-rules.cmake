if(PROJECT_IS_TOP_LEVEL)
	set(
		CMAKE_INSTALL_INCLUDEDIR "include/ini_manager-${PROJECT_VERSION}"
		CACHE STRING ""
	)
	set_property(CACHE CMAKE_INSTALL_INCLUDEDIR PROPERTY TYPE PATH)
endif()

# Header-only package: use a platform-independent config install directory
set(CMAKE_INSTALL_LIBDIR lib CACHE PATH "")

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# find_package(<package>) call for consumers to find this project
set(package ini_manager)

install(TARGETS ini_manager_ini_manager EXPORT ini_managerTargets
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT ini_manager_Development)

install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/ini_manager"
    COMPONENT ini_manager_Development)

write_basic_package_version_file(
	"${package}ConfigVersion.cmake"
	COMPATIBILITY SameMajorVersion
	ARCH_INDEPENDENT
)

# Allow package maintainers to freely override the path for the configs
set(
	ini_manager_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
	CACHE STRING "CMake package config location relative to the install prefix"
)
set_property(CACHE ini_manager_INSTALL_CMAKEDIR PROPERTY TYPE PATH)
mark_as_advanced(ini_manager_INSTALL_CMAKEDIR)

install(
	FILES cmake/install-config.cmake
	DESTINATION "${ini_manager_INSTALL_CMAKEDIR}"
	RENAME "${package}Config.cmake"
	COMPONENT ini_manager_Development
)

install(
	FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
	DESTINATION "${ini_manager_INSTALL_CMAKEDIR}"
	COMPONENT ini_manager_Development
)

install(
	EXPORT ini_managerTargets
	NAMESPACE ini_manager::
	DESTINATION "${ini_manager_INSTALL_CMAKEDIR}"
	COMPONENT ini_manager_Development
)

if(PROJECT_IS_TOP_LEVEL)
    set(CPACK_PACKAGE_FILE_NAME "ini_manager-${PROJECT_VERSION}")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_PACKAGE_CONTACT "LineGM@yandex.ru")
    include(CPack)
endif()
