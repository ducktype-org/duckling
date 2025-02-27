#include "validator.hpp"
#include <iostream>

namespace vm::validator {
    bool vm::validator::Validator::validateProgram([[maybe_unused]] const vm::VMProgram& program) {
        std::cout << "Validating code\n";
        return true;
    }
}