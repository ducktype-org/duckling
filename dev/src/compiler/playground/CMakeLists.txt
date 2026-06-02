# Playground
duck_add_playground(char char_playground.cpp USES Lexer Diagnostic Init)
duck_add_playground(file file_playground.cpp USES Filesystem Init)
duck_add_playground(lexer lexer_playground.cpp USES Lexer TokenSource Init)
duck_add_playground(elements elements_playground.cpp USES Lexer PstParser Init)
duck_add_playground(pst_graph pst_graph_playground.cpp USES Lexer PstParser System::gvc System::cgraph System::cdt Init)
duck_add_playground(frontend frontend_playground.cpp USES Filesystem ModuleTree Clah Init)
duck_add_playground(helios helios_playground.cpp USES Base ModuleTree Clah HELIOS Init)
duck_add_playground(main_code_dependency main_code_dependency_playground.cpp USES Base ModuleTree Clah HELIOS Init)
