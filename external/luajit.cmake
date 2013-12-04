# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set(INSTALL_DIR ${HAKA_INSTALL_PREFIX})
set(INSTALL_FULLDIR ${CMAKE_HAKA_INSTALL_PREFIX})

set(LUAJIT_CFLAGS -DLUAJIT_ENABLE_LUA52COMPAT)
set(LUAJIT_CCDEBUG "")

if(NOT ${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "x86_64")
	# Force external unwinding
	# Otherwise we get conflicts: when doing a pthread_cancel, the cancel request
	# is caught by luajit on a pcall and the system ends up in corrupted state.
	# WARNING: When enabling this all C code need to be compiled with funwind-tables
	# (or -fexceptions), see luajit/src/lj_err.c for more details.
	set(LUAJIT_CFLAGS ${LUAJIT_CFLAGS} -funwind-tables -DLUAJIT_UNWIND_EXTERNAL)
endif()

if(CMAKE_BUILD_TYPE STREQUAL Debug)
	set(LUAJIT_CFLAGS ${LUAJIT_CFLAGS} -O0 -g -DLUA_USE_APICHECK) # -DLUAJIT_USE_GDBJIT -DLUA_USE_ASSERT
	set(LUAJIT_CCDEBUG ${LUAJIT_CCDEBUG} -g)
elseif(CMAKE_BUILD_TYPE STREQUAL Memcheck)
	set(LUAJIT_CFLAGS ${LUAJIT_CFLAGS} -O0 -g -DLUAJIT_USE_VALGRIND -DLUA_USE_APICHECK) # -DLUAJIT_USE_GDBJIT
	set(LUAJIT_CCDEBUG ${LUAJIT_CCDEBUG} -g)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	set(LUAJIT_CFLAGS ${LUAJIT_CFLAGS} -g)
	set(LUAJIT_CCDEBUG ${LUAJIT_CCDEBUG} -g)
else()
	set(LUAJIT_CFLAGS ${LUAJIT_CFLAGS} -O2)
endif()

execute_process(COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/luajit)
execute_process(COMMAND echo CFLAGS="${LUAJIT_CFLAGS}" CCDEBUG="${LUAJIT_CCDEBUG}"
	PREFIX=${INSTALL_FULLDIR} OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.tmp)
execute_process(COMMAND cmp ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.tmp ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.build
	RESULT_VARIABLE FILE_IS_SAME OUTPUT_QUIET ERROR_QUIET)
if(FILE_IS_SAME)
	execute_process(COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.tmp ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.opt)
endif(FILE_IS_SAME)

add_custom_target(luajit-sync
	COMMAND rsync -rt ${CMAKE_CURRENT_SOURCE_DIR}/luajit ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.build
	COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/luajit PREFIX=${INSTALL_FULLDIR} clean
	COMMAND cp ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.opt ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.build
	MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.opt
	DEPENDS luajit-sync
)

add_custom_target(luajit
	COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/luajit PREFIX=${INSTALL_FULLDIR} BUILDMODE=dynamic
		CFLAGS="${LUAJIT_CFLAGS}" CCDEBUG="${LUAJIT_CCDEBUG}"
	COMMAND make -C ${CMAKE_CURRENT_BINARY_DIR}/luajit LDCONFIG='/sbin/ldconfig -n' PREFIX=${INSTALL_FULLDIR}
		BUILDMODE=dynamic CFLAGS="${LUAJIT_CFLAGS}" CCDEBUG="${LUAJIT_CCDEBUG}"
		INSTALL_X='install -m 0755 -p' INSTALL_F='install -m 0644 -p' DESTDIR=${CMAKE_CURRENT_BINARY_DIR}/luajit install
	DEPENDS luajit-sync ${CMAKE_CURRENT_BINARY_DIR}/luajit/cmake.build
)

if(LUA STREQUAL "luajit")
	set(LUA_DIR ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR} PARENT_SCOPE)
	set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/include/luajit-2.0 PARENT_SCOPE)
	set(LUA_LIBRARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/lib/ PARENT_SCOPE)
	set(LUA_LIBRARIES ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/lib/libluajit-5.1.so PARENT_SCOPE)

	set(LUA_COMPILER ${CMAKE_CURRENT_SOURCE_DIR}/luajitc -p "${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/" PARENT_SCOPE)
	set(LUA_FLAGS_NONE "-g" PARENT_SCOPE)
	set(LUA_FLAGS_DEBUG "-g" PARENT_SCOPE)
	set(LUA_FLAGS_MEMCHECK "-g" PARENT_SCOPE)
	set(LUA_FLAGS_RELEASE "-s" PARENT_SCOPE)
	set(LUA_FLAGS_RELWITHDEBINFO "-g" PARENT_SCOPE)
	set(LUA_FLAGS_MINSIZEREL "-s" PARENT_SCOPE)

	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/lib DESTINATION ${INSTALL_DIR})
	install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/luajit/${INSTALL_FULLDIR}/share DESTINATION ${INSTALL_DIR})
endif()
