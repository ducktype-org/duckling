#pragma once

#include <code_data/program.hpp>
#include "../parser/parser.hpp"


namespace vm::validator {
class Validator {
     public:
        Validator() = default;

        bool validateProgram(const vm::VMProgram& program);

    private:
        vm::VMProgram program;
};

}