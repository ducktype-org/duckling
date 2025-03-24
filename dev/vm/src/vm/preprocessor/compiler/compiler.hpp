#include "diagnostic/logger.hpp"
#include "vm/core/thread/low_program/low_program.hpp"
#include "vm/preprocessor/preprocessor.hpp"
#include "vm/program/program.hpp"

namespace vm::compiler {
	std::expected<vm::low::LowVMProgram, PreprocessorLogger>
		compile(const program::Program& program, OptPosMapCRef pos_map);
}
