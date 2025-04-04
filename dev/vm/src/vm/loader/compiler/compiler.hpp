#pragma once
#include <vm/loader/loader.hpp>

namespace vm::compiler {
	std::expected<low::LowVMProgram, LoaderLogger> compile(const Program& program);
}
