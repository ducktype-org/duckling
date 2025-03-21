#include "diagnostic/logger.hpp"
#include "vm/core/thread/low_program/low_program.hpp"
#include "vm/program/program.hpp"

namespace vm::compiler {
	std::expected<vm::low::LowVMProgram, dia::Logger> compile(program::Program program);
}
