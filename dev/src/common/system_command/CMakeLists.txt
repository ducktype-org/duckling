add_library(SystemCommand
        src/system_command/system_command.cpp
)

make_module(SystemCommand USES Base StringID Logger OSUtils)

duck_add_test(common system_command tests/system_command_test.cpp USES SystemCommand Base)
