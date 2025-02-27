add_library(SystemCommand
        src/system_command/system_command.cpp
)

make_module(SystemCommand USES Base)

duck_add_test(common system_command_test tests/tests.cpp
	USES SystemCommand Base)
