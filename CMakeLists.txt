cmake_minimum_required(VERSION 3.16)

project(xpilot-fxi
    VERSION 1.4.6
    LANGUAGES C
)

include(GNUInstallDirs)
include(CheckFunctionExists)

option(XPILOT_ENABLE_PLOCKSERVER "Lock the server into memory" OFF)
option(XPILOT_ENABLE_DEVELOPMENT "Enable development code" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(PACKAGE "xpilot-fxi")
set(PACKAGE_NAME "XPilot FXI")
set(PACKAGE_TARNAME "xpilot-fxi")
set(PACKAGE_VERSION "1.4.6fxi")
set(PACKAGE_STRING "XPilot FXI 1.4.6fxi")
set(PACKAGE_BUGREPORT "kps@users.sourceforge.net")
set(PACKAGE_URL "https://github.com/ksoderbl/xpilot-fxi")
set(VERSION "1.4.6fxi")

set(XP_AUTHORS "Bjoern Stabell, Ken Ronny Schouten, Bert Gijsbers, Dick Balaska, Kristian Soderblom, et al")
set(XP_COPYRIGHTDATE "1991-2025")
set(XP_COPYRIGHT "Copyright (C) ${XP_COPYRIGHTDATE} by ${XP_AUTHORS}")
set(XP_RELEASEDATE "Apr 1, 2009")

# Install data under share/xpilot-fxi
set(XPILOT_PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/xpilot-fxi")
set(XP_DATADIR "${XPILOT_PKGDATADIR}")

check_function_exists(strlcpy HAVE_STRLCPY)
check_function_exists(strlcat HAVE_STRLCAT)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/src/common/version.h
    @ONLY
)

find_package(EXPAT REQUIRED)

if(UNIX)
    find_library(M_LIBRARY m)
endif()

# ---- common library ----

file(GLOB COMMON_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.c"
)

add_library(xpcommon STATIC ${COMMON_C_SOURCES})

target_include_directories(xpcommon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/src/common
)

target_compile_definitions(xpcommon
    PUBLIC
        CONF_DATADIR="${XPILOT_PKGDATADIR}/"
)

if(XPILOT_ENABLE_DEVELOPMENT)
    target_compile_definitions(xpcommon PUBLIC DEVELOPMENT=1)
endif()

target_link_libraries(xpcommon
    PUBLIC
        EXPAT::EXPAT
)

if(M_LIBRARY)
    target_link_libraries(xpcommon PUBLIC ${M_LIBRARY})
endif()

# ---- server executable ----

file(GLOB SERVER_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/server/*.c"
)

add_executable(xpilot-fxi-server ${SERVER_C_SOURCES})

target_include_directories(xpilot-fxi-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/server
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/src/common
)

target_compile_definitions(xpilot-fxi-server
    PRIVATE
        CONF_DATADIR="${XPILOT_PKGDATADIR}/"
)

if(XPILOT_ENABLE_PLOCKSERVER)
    target_compile_definitions(xpilot-fxi-server PRIVATE PLOCKSERVER=1)
endif()

if(XPILOT_ENABLE_DEVELOPMENT)
    target_compile_definitions(xpilot-fxi-server PRIVATE DEVELOPMENT=1)
endif()

target_link_libraries(xpilot-fxi-server
    PRIVATE
        xpcommon
)

if(M_LIBRARY)
    target_link_libraries(xpilot-fxi-server PRIVATE ${M_LIBRARY})
endif()

install(TARGETS xpilot-fxi-server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---- shared runtime data ----

file(GLOB SHARE_ROOT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/share/*"
)

list(FILTER SHARE_ROOT_FILES EXCLUDE REGEX "/Makefile\\.(am|in)$")
list(FILTER SHARE_ROOT_FILES EXCLUDE REGEX "/maps$")

install(FILES ${SHARE_ROOT_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xpilot-fxi
)

file(GLOB SHARE_MAP_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/share/maps/*"
)

list(FILTER SHARE_MAP_FILES EXCLUDE REGEX "/Makefile\\.(am|in)$")

install(FILES ${SHARE_MAP_FILES}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xpilot-fxi/maps
)
