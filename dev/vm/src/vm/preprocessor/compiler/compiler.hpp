#include <vm/preprocessor/preprocessor.hpp>

namespace vm::compiler {
	std::expected<vm::low::LowVMProgram, PreprocessorLogger> compile(const Program& program);
}
