add_library(std MODULE std.c math.c std.crisp.o)
set_target_properties(std PROPERTIES SUFFIX ".crisp.so")
add_custom_command(OUTPUT std.crisp COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/std.crisp MAIN_DEPENDENCY std.crisp)
add_custom_command(OUTPUT std.crisp.o COMMAND ld -r -b binary -o std.crisp.o std.crisp MAIN_DEPENDENCY std.crisp)
install(TARGETS std DESTINATION lib)
