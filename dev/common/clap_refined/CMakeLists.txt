add_library(Clap2
        src/clap/clap.cpp
        src/clap/clap.hpp
        src/clap/clap_parameter.cpp
        src/clap/clap_parameter.hpp
        src/clap/value_parser.cpp
        src/clap/value_parser.hpp
        src/clap/parsing_result.cpp
        src/clap/parsing_result.hpp
        src/clap/param_builder.hpp
        src/clap/param_builder.cpp)

make_module(Clap2 USES Base Filesystem)

rift_add_test(common clap_tester tests/clap_tests.cpp
        USES Clap2 Base)

rift_add_test(common clap_parser_tester tests/clap_parser_tests.cpp
        USES Clap2 Base)
