duck_add_playground(example_vm example_vm_playground.cpp USES LibVM Base Clah Crow::Crow Filesystem TPC)
duck_add_playground(get_types get_types_playground.cpp USES LibVM)
target_include_directories(example_vm_playground PRIVATE ../src)
