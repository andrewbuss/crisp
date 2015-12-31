#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "modules/sdl2/sdl2-cmake-scripts")

find_package(SDL2 REQUIRED)

add_library(sdl2 MODULE sdl2.crisp.o sdl2.c)
set_target_properties(sdl2 PROPERTIES SUFFIX ".crisp.so")
add_custom_command(OUTPUT sdl2.crisp COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/sdl2.crisp MAIN_DEPENDENCY sdl2.crisp)
add_custom_command(OUTPUT sdl2.crisp.o COMMAND ld -r -b binary -o sdl2.crisp.o sdl2.crisp MAIN_DEPENDENCY sdl2.crisp)
include_directories(${SDL2_INCLUDE_DIR}
        ${SDL2_IMAGE_INCLUDE_DIR}
        ${SDL2_TTF_INCLUDE_DIR})
target_link_libraries(sdl2 SDL2
        ${SDL2_IMAGE_LIBRARIES}
        ${SDL2_TTF_LIBRARIES})
install(TARGETS sdl2 DESTINATION lib)
