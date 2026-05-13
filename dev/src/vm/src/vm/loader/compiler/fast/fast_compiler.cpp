#include "fast_compiler.hpp"

using namespace vm::code;
using namespace vm::loader;
using namespace vm::loader::compiler;

compiler::fast::FastCompiler::FastCompiler(const ValidProgram& high_program):
	  Compiler(high_program) {}

CRef<vm::fast::ProgramBase> vm::loader::compiler::fast::FastCompiler::getProgramBase() const {
	return &program;
}

void fast::FastCompiler::compileNewTypes(const std::vector<valid_type::ValidType>& new_types) {}

void fast::FastCompiler::compileNewGlobals(const std::vector<GlobalData>& new_globals) {}

void fast::FastCompiler::compileNewFunctions(const std::vector<Function>& new_functions) {}

void fast::FastCompiler::compileNewExtCFunctions(
	const std::vector<code::ExternalCFunction>& new_functions
) {}

ProgramSize fast::FastCompiler::getCurrentProgramSize() const {
	return ProgramSize{
		.function_count       = program.functions.size(),
		.global_count         = program.global_data.size(),
		.type_count           = program.types.size(),
		.ext_c_function_count = program.extern_c_functions.size(),
	};
}
