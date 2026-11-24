cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME_VAR "THUG_SDL")
project(${PROJECT_NAME_VAR})

set(SRC "${CMAKE_SOURCE_DIR}/src")

set(SOURCE_FILES
	"${SRC}/main.cpp"
	"${SRC}/exports.def"
	"${SRC}/Resources/Resources.rc"
	"${SRC}/patch.cpp"
	"${SRC}/config.cpp"
	"${SRC}/input.cpp"
	"${SRC}/flickit.cpp"
	"${SRC}/flickit_game.cpp"
	"${SRC}/memory.cpp"
	"${SRC}/bigworld.cpp"
	"${SRC}/camrel_game.cpp"
	"${SRC}/worldmap_game.cpp"
	"${SRC}/drive_game.cpp"
	"${SRC}/players_game.cpp"
	"${SRC}/guard_game.cpp"
	"${SRC}/script.cpp"
	"${SRC}/displayconfig.cpp"
	"${SRC}/observemenu.cpp"
	"${SRC}/Logger/Logger.cpp"
	"${SRC}/Logger/ErrorManager.cpp"
	"${SRC}/QB/LazyStruct.cpp"
	"${SRC}/QB/LazyArray.cpp"
	"${SRC}/QB/QB.cpp"
	"${SRC}/QB/QBKey.cpp"
	"${SRC}/QB/CFuncs.cpp"
	"${SRC}/Mod/mod_template.cpp"
	"${SRC}/modloader.cpp"
	"${SRC}/event.cpp"
	"${SRC}/Util/sb.cpp"
)

set(HEADER_FILES
	"${SRC}/Resources/Resources.h"
	"${SRC}/d3d9.h"
	"${SRC}/patch.h"
	"${SRC}/config.h"
	"${SRC}/input.h"
	"${SRC}/flickit.h"
	"${SRC}/flickit_game.h"
	"${SRC}/memory.h"
	"${SRC}/bigworld.h"
	"${SRC}/camrel.h"
	"${SRC}/camrel_game.h"
	"${SRC}/worldmap.h"
	"${SRC}/worldmap_game.h"
	"${SRC}/drive.h"
	"${SRC}/drive_game.h"
	"${SRC}/flickit_cars_qb.h"
	"${SRC}/players.h"
	"${SRC}/players_game.h"
	"${SRC}/flickit_players_qb.h"
	"${SRC}/nodenames.h"
	"${SRC}/autosave.h"
	"${SRC}/watchdog.h"
	"${SRC}/guard.h"
	"${SRC}/guard_game.h"
	"${SRC}/displayconfig.h"
	"${SRC}/observemenu.h"
	"${SRC}/script.h"
	"${SRC}/keycodes.h"
	"${SRC}/Logger/Logger.h"
	"${SRC}/Logger/ErrorManager.h"
	"${SRC}/QB/LazyStruct.h"
	"${SRC}/QB/LazyArray.h"
	"${SRC}/QB/malloc.h"
	"${SRC}/QB/QB.h"
	"${SRC}/QB/QBKey.h"
	"${SRC}/QB/CFuncs.h"
	"${SRC}/QB/scriptcontent.h"
	"${SRC}/Mod/mod_template.h"
	"${SRC}/modloader.h"
	"${SRC}/event.h"
	"${SRC}/Util/sb.h"
)

find_package(SDL2 REQUIRED)
list(APPEND LIBS
	SDL2::SDL2
	SDL2::SDL2main
)
list(APPEND INCLUDES ${SDL2_INCLUDE_DIRS})

add_library(${PROJECT_NAME_VAR} SHARED)

target_sources(${PROJECT_NAME_VAR} PRIVATE
	${SOURCE_FILES}
	${HEADER_FILES}
)

target_link_libraries(${PROJECT_NAME_VAR} PUBLIC ${LIBS} dbghelp)

target_include_directories(${PROJECT_NAME_VAR} PUBLIC
	"${PROJECT_SOURCE_DIR}/src"
	${INCLUDES}
)

set_source_files_properties(exports.def PROPERTIES HEADER_FILE_ONLY TRUE)

set_target_properties(
    ${PROJECT_NAME_VAR} 
    PROPERTIES 
        OUTPUT_NAME "thugsdl"
)

# A map of the DLL next to it (thugsdl.map), for telling which function an address in it is in
if(MSVC)
	set_property(TARGET ${PROJECT_NAME_VAR} APPEND_STRING PROPERTY LINK_FLAGS " /MAP")
endif()

# The guard's breakpoint tried on Windows itself, outside the game (the CI runs it): not part of the DLL
if(WIN32)
	add_executable(guard_windows_test EXCLUDE_FROM_ALL "${CMAKE_SOURCE_DIR}/tests/guard_windows_test.cpp")
	target_include_directories(guard_windows_test PRIVATE "${SRC}")
endif()

add_executable(tsdl-launcher)

set(LSRC "${CMAKE_SOURCE_DIR}/launcher")

set(LSOURCE_FILES
	"${LSRC}/main.c"
	"${LSRC}/pgui.c"
	"${LSRC}/pagegeneral.c"
	"${LSRC}/pagekeyboard.c"
	"${LSRC}/pagegamepad.c"
	"${LSRC}/pagecontrol.c"
	"${LSRC}/Resources.rc"
)

set (LHEADER_FILES
	"${LSRC}/pgui.h"
	"${LSRC}/pagegeneral.h"
	"${LSRC}/pagekeyboard.h"
	"${LSRC}/pagegamepad.h"
	"${LSRC}/pagecontrol.h"
	"${LSRC}/global.h"
	"${LSRC}/Resources.h"
)

target_sources(tsdl-launcher PUBLIC 
	${LSOURCE_FILES}
	${LHEADER_FILES}
)
target_link_libraries(tsdl-launcher PRIVATE 
	${LIBS}
	comctl32.lib
	UxTheme.lib
)
target_include_directories(tsdl-launcher PUBLIC
	"${PROJECT_SOURCE_DIR}/src"
	${INCLUDES}
)

set_target_properties(tsdl-launcher 
	PROPERTIES 
	    OUTPUT_NAME "THUG-SDL Launcher"
)

add_executable(thugsdlpatcher)

target_sources(thugsdlpatcher PUBLIC
	"${CMAKE_SOURCE_DIR}/patcher/main.c"
)

target_include_directories(thugsdlpatcher PUBLIC
	"${CMAKE_SOURCE_DIR}/src"
	${THIRDPARTY}
)