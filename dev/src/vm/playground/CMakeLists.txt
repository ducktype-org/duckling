duck_add_playground(example_vm example_vm_playground.cpp USES LibVM Base Clah Crow::Crow Filesystem TPC)
duck_add_playground(state_machine state_machine_playground.cpp USES LibVM Base)
target_include_directories(example_vm_playground PRIVATE ../src)
