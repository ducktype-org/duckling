#pragma once
#include <vm/loader/loader.hpp>

namespace vm::loader::compiler {
	/**
	 * @brief Transforms high-level Program into low-level program.
	 */
	low::LowVMProgram compile(const code::ValidProgram& program);
}
