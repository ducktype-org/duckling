add_library(Clap2
        src/clap/clap.cpp
        src/clap/clap.hpp
        src/clap/config_parameter.cpp
        src/clap/config_parameter.hpp
        src/clap/value_parser.cpp
        src/clap/value_parser.hpp
        src/clap/parsing_result.cpp
        src/clap/parsing_result.hpp
        src/clap/param_builder.hpp
        src/clap/param_builder.cpp)

make_module(Clap2 USES Base Filesystem)

rift_add_test(common clap_test_param_builder tests/param_builder_test.cpp
        USES Clap2 Base)
