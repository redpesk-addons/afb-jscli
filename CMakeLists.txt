# Copyright (C) 2019-2022 IoT.bzh Company
# Author: José Bollo <jose.bollo@iot.bzh>
###########################################
cmake_minimum_required(VERSION 3.13)

project(afb-jscli VERSION 0.3.0 LANGUAGES C)

include(GNUInstallDirs)
include(FindPkgConfig)

pkg_check_modules(AFBCLI REQUIRED IMPORTED_TARGET libafbcli>=4.1 libsystemd)

set(QJSDIR ${CMAKE_SOURCE_DIR}/quickjs)
set(MODDIR ${CMAKE_SOURCE_DIR}/modules)
set(MODPATH ${CMAKE_INSTALL_FULL_DATADIR}/afb-jscli/modules)

add_library(qjs OBJECT quickjs/quickjs.c quickjs/libregexp.c quickjs/libunicode.c quickjs/cutils.c quickjs/quickjs-libc.c)
target_compile_definitions(qjs PRIVATE _GNU_SOURCE CONFIG_VERSION="2021-03-27")

add_executable(afb-jscli afb-jscli.c)
set_target_properties(afb-jscli PROPERTIES ENABLE_EXPORTS TRUE)
target_link_libraries(afb-jscli qjs -lm -ldl -lpthread)
target_include_directories(afb-jscli PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(afb-jscli PRIVATE MODPATH="${MODPATH}")

add_library(afb-qjs SHARED modules/afb/afb-qjs.c modules/afb/afbwsj1-qjs.c modules/afb/afbwsapi-qjs.c)
target_include_directories(afb-qjs PRIVATE ${CMAKE_SOURCE_DIR} ${AFBCLI_INCLUDE_DIRS})
target_link_libraries(afb-qjs PkgConfig::AFBCLI afb-jscli)

install(TARGETS afb-jscli DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

set(MODSJS 
	${MODDIR}/system.js
	${MODDIR}/libafbws.js
	${MODDIR}/diag.js
)

install(FILES ${MODSJS} DESTINATION ${MODPATH})
install(FILES ${MODDIR}/afb/index.js DESTINATION ${MODPATH}/afb)
install(TARGETS afb-qjs DESTINATION ${MODPATH}/afb)

