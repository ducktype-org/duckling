#pragma once
#include <vm/loader/loader.hpp>

namespace vm::loader::compiler {
	/**
	 * @brief Transforms high-level Program into low-level program.
	 * @TODO: Currently has an assumption about stack variables.
	 */
	low::LowVMProgram compile(const code::ValidProgram& program);
}
