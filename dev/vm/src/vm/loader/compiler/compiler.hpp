#pragma once
#include <vm/loader/loader.hpp>

namespace vm::compiler {
	std::expected<low::LowVMProgram, PreprocessorLogger> compile(const Program& program);
}
