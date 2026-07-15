add_library(StateMachine
	src/state_machine/state_machine.hpp
	src/state_machine/state_machine.cpp
)

make_module(StateMachine USES Base)

add_example(state_machine_example examples/state_machine_example.cpp USES StateMachine)

duck_add_test(common state_machine tests/state_machine_test.cpp USES StateMachine)
