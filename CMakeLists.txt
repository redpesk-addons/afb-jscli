cmake_minimum_required(VERSION 3.13)

project(afb-jscli VERSION 0.2.0 LANGUAGES C)

include(GNUInstallDirs)
include(FindPkgConfig)

pkg_check_modules(AFBCLI REQUIRED IMPORTED_TARGET libafbcli libsystemd)

set(QJSDIR ${CMAKE_CURRENT_SOURCE_DIR}/quickjs)
set(MODDIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)
set(MODPATH ${CMAKE_INSTALL_FULL_DATADIR}/afb-jscli/modules)

add_custom_command(
	OUTPUT ${QJSDIR}/libquickjs.a
	COMMAND make -C ${QJSDIR} prefix=$(CMAKE_INSTALL_PREFIX) CONFIG_BIGNUM= libquickjs.a
)

add_executable(afb-jscli afb-jscli.c)
target_link_libraries(afb-jscli ${QJSDIR}/libquickjs.a -lm -ldl -lpthread)
target_include_directories(afb-jscli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(afb-jscli PRIVATE MODPATH="${MODPATH}")

add_library(afb-qjs SHARED modules/afb/afb-qjs.c modules/afb/afbwsj1-qjs.c modules/afb/afbwsapi-qjs.c)
target_include_directories(afb-qjs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${AFBCLI_INCLUDE_DIRS})
target_link_libraries(afb-qjs PkgConfig::AFBCLI)

install(TARGETS afb-jscli DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

set(MODSJS 
	${MODDIR}/system.js
	${MODDIR}/libafbws.js
	${MODDIR}/diag.js
)

install(FILES ${MODSJS} DESTINATION ${MODPATH})
install(FILES ${MODDIR}/afb/index.js DESTINATION ${MODPATH}/afb)
install(TARGETS afb-qjs DESTINATION ${MODPATH}/afb)

