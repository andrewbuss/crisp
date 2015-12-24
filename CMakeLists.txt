cmake_minimum_required(VERSION 2.6)

project(crisp C)
find_package(Threads)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DPOINTER_MASK=0x0000FFFFFFFFFFFF)
add_subdirectory(bdwgc EXCLUDE_FROM_ALL)
add_subdirectory(modules/map)
add_subdirectory(modules/std)

include_directories(bdwgc/include)

add_executable(crisp_fuzz EXCLUDE_FROM_ALL crisp.c interpreter.c parse.c)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS DISABLE_FFI=1)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_malloc=malloc)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_malloc_atomic=malloc)
set_property(TARGET crisp_fuzz APPEND PROPERTY COMPILE_DEFINITIONS GC_realloc=realloc)

add_executable(crisp crisp.c interpreter.c ffi.c parse.c)
target_link_libraries(crisp dl gc-lib ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS crisp DESTINATION bin)
