#pragma once

#include <expected>
#include <string>
#include "helios/ctv/ctv.hpp"
#include "helios/helios_errors.hpp"
namespace compiler::helios {
    class CompileTimeEvaluator {
        public:
            static CompileTimeEvaluator& get();
            std::expected<CTV, errors::Failed> executeInVm(
                const vm::code::CodeCollection& code,
                const std::string& func_name,
                const std::vector<CTV>& args
            );
        private:
            CompileTimeEvaluator();
            ~CompileTimeEvaluator();
            CompileTimeEvaluator(const CompileTimeEvaluator&) = delete;
            CompileTimeEvaluator& operator=(const CompileTimeEvaluator&) = delete;


    };
}