#pragma once
#include <vm/loader/loader.hpp>

namespace vm::loader::compiler {
	/**
	 * @brief Transforms high-level Program into low-level program.
	 */
	std::expected<low::LowVMProgram, LoaderLogger> compile(const Program& program);
}
