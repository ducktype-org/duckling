duck_add_playground(parser_f8 parser_f8_playground.cpp USES LibVM Base Listener Clap Crow::Crow Filesystem TPC)
duck_add_playground(vm_repl vm_repl_playground.cpp USES LibVM Base Filesystem)
target_include_directories(parser_f8_playground PRIVATE ../src)
target_include_directories(vm_repl_playground PRIVATE ../src)