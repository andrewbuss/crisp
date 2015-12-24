add_library(map MODULE map.c map.crisp.o)
set_target_properties(map PROPERTIES SUFFIX ".crisp.so")
add_custom_command(OUTPUT map.crisp.o
        COMMAND ln -fs ${CMAKE_CURRENT_SOURCE_DIR}/map.crisp && ld -r -b binary -o map.crisp.o map.crisp
        MAIN_DEPENDENCY map.crisp)
install(TARGETS map DESTINATION lib)
