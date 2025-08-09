#pragma once

#include "helios_private/comp_time/vm_evaluator.hpp"
#include <expected>
#include <string>
#include "base/exceptions.hpp"
#include "helios/ctv/ctv.hpp"
#include "helios/helios_errors.hpp"

namespace compiler::helios {
    CompileTimeEvaluator& CompileTimeEvaluator::get() {
        static CompileTimeEvaluator instance;
        return instance;
    } 
    
    CompileTimeEvaluator::CompileTimeEvaluator() {
        throw base::NotYetImplemented("Not implemented");
    }

    CompileTimeEvaluator::~CompileTimeEvaluator() {
        throw base::NotYetImplemented("Not implemented");
    }

    std::expected<CTV, errors::Failed> CompileTimeEvaluator::executeInVm(
        const vm::code::CodeCollection& code,
        const std::string& func_name,
        const std::vector<CTV> args
    ) {
        throw base::NotYetImplemented("Not implemented");
    }
}