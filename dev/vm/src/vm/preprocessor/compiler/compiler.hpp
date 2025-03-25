#include "diagnostic/logger.hpp"
#include "vm/core/thread/low_program/low_program.hpp"
#include "vm/preprocessor/preprocessor.hpp"
#include "vm/code/code.hpp"

namespace vm::compiler {
	std::expected<vm::low::LowVMProgram, PreprocessorLogger> compile(const Program& program);
}
