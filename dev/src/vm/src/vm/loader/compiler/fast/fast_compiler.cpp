#include "fast_compiler.hpp"

using namespace vm;
using namespace vm::code;
using namespace vm::loader;
using namespace vm::loader::compiler;

fast::FastCompiler::FastCompiler(const ValidProgram& high_program): Compiler(high_program) {}

void fast::FastCompiler::compileNewTypes(const std::vector<valid_type::ValidType>& new_types) {}

void fast::FastCompiler::compileNewGlobals(
	const std::vector<GlobalData>& new_globals
) {
}

void fast::FastCompiler::compileNewFunctions(
	const std::vector<Function>& new_functions
) {
}

void vm::loader::compiler::fast::FastCompiler::compileNewExtCFunctions(
	const std::vector<code::ExternalCFunction>& new_functions
) {
}

vm::loader::compiler::ProgramSize vm::loader::compiler::fast::FastCompiler::getCurrentProgramSize() const {

}
