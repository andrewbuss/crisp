cmake_minimum_required(VERSION 3.0)

project(crisp LANGUAGES C)
find_package(Threads)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-DPOINTER_MASK=0x0000FFFFFFFFFFFF)
add_subdirectory(bdwgc EXCLUDE_FROM_ALL)

add_executable(crisp_noffi EXCLUDE_FROM_ALL crisp.c interpreter.c parse.c math.c)
target_compile_definitions(crisp_noffi PRIVATE DISABLE_FFI)
target_link_libraries(crisp_noffi gc-lib ${CMAKE_THREAD_LIBS_INIT})

add_executable(crisp_fuzz_parser EXCLUDE_FROM_ALL fuzzing/fuzz_parser.c crisp.c parse.c math.c )
target_compile_definitions(crisp_fuzz_parser PRIVATE DISABLE_FFI)
target_compile_definitions(crisp_fuzz_parser PUBLIC GC_malloc=malloc GC_malloc_atomic=malloc GC_realloc=realloc)

add_executable(crisp_fuzz EXCLUDE_FROM_ALL crisp.c interpreter.c parse.c math.c )
target_compile_definitions(crisp_fuzz PRIVATE DISABLE_FFI)
target_compile_definitions(crisp_fuzz PUBLIC GC_malloc=malloc GC_malloc_atomic=malloc GC_realloc=realloc)

add_executable(crisp crisp.c interpreter.c ffi.c parse.c math.c)
target_link_libraries(crisp dl gc-lib ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS crisp DESTINATION bin)
