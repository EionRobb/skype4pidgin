cmake_minimum_required (VERSION 2.8)

set(PROJECT_NAME "skypeweb")
set(VERSION_MAJOR "1")
set(VERSION_MINOR "1")
set(VERSION_PATCH "0")

project(${PROJECT_NAME})

set(CMAKE_MODULE_PATH 
    ${CMAKE_SOURCE_DIR}/cmake 
    ${CMAKE_MODULE_PATH}
    )

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED  glib-2.0)
pkg_check_modules(JSON-GLIB REQUIRED json-glib-1.0)
pkg_check_modules(PURPLE REQUIRED purple)

add_definitions(-Wall)

include_directories(
    ${PURPLE_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
    ${JSON-GLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

set(SRC_LIST
    skypeweb_connection.c
    skypeweb_contacts.c
    skypeweb_login.c
    skypeweb_messages.c
    skypeweb_util.c
    libskypeweb.c 
    )

add_library(${PROJECT_NAME} SHARED ${SRC_LIST})

target_link_libraries(${PROJECT_NAME}
    ${PURPLE_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${JSON-GLIB_LIBRARIES}
    )

link_directories(
    ${PURPLE_LIBRARY_DIRS}
    )


#install(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_INSTALL_DIR})
exec_program("${PKG_CONFIG_EXECUTABLE} --variable=plugindir purple 2>/dev/null"
        OUTPUT_VARIABLE LIB_INSTALL_DIR)

install(TARGETS ${PROJECT_NAME} DESTINATION ${LIB_INSTALL_DIR})

exec_program("${PKG_CONFIG_EXECUTABLE} --variable=datadir purple 2>/dev/null"
        OUTPUT_VARIABLE PURPLE_DATADIR)

install(DIRECTORY "icons/"
    DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/protocols/"
    )

install(FILES "theme"
    DESTINATION "${PURPLE_DATADIR}/pixmaps/pidgin/emotes/skype/"
    )

# package settings
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Skype protocol plug-in for libpurple")
set(CPACK_PACKAGE_VENDOR "Eionrobb")
set(CPACK_PACKAGE_DESCRIPTION "libskypeweb is a Skype protocol plug-in for libpurple based on the Skype Web client")
set(CPACK_PACKAGE_CONTACT "edhelas@movim.eu")
set(CPACK_PACKAGE_VERSION_MAJOR "${VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${VERSION_PATCH}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) 
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${VERSION}")
set(ACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${VERSION}")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libpurple0 (>= 2.10.0), libglib2.0-0 (>= 2.24), libjson-glib-1.0-0 (>= 0.8.0)")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Jaussoin Timothée") #required

set(PACK "DEB" CACHE STRING "Generate a Package")
set(CPACK_GENERATOR ${PACK})

include(CPack)
