cmake_minimum_required(VERSION 2.6)

project(crisp C)
find_package(Threads)


add_definitions(-DPOINTER_MASK=0x0000FFFFFFFFFFFF)
add_subdirectory(bdwgc EXCLUDE_FROM_ALL)

include_directories(bdwgc/include)
include_directories(.)

add_subdirectory(modules/map)
add_subdirectory(modules/std)
add_subdirectory(modules/queue)
add_subdirectory(modules/strict-test)
add_subdirectory(modules/sdl2)

add_executable(crisp_fuzz EXCLUDE_FROM_ALL crisp.c interpreter.c parse.c)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS DISABLE_FFI=1)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS FUZZ=1)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_malloc=malloc)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_malloc_atomic=malloc)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_realloc=realloc)

add_executable(crisp crisp.c interpreter.c ffi.c parse.c)
target_link_libraries(crisp dl gc-lib ${CMAKE_THREAD_LIBS_INIT})

set(CMAKE_BUILD_TYPE  DEBUG)

add_executable(crisp_debug crisp.c interpreter.c ffi.c parse.c)
target_link_libraries(crisp_debug dl gc-lib ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS crisp DESTINATION bin)
