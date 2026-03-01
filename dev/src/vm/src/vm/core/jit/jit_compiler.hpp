/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <vm/core/thread/low_program/low_program.hpp>

namespace vm {
	using JitOpFun = void(const vm::MicroInstruction**, std::byte**, vm::Frame**, vm::VMThread*);

	/**
	 * @brief The data additionally stored per function, by the JIT compiler
	 */
	struct JitFuncData {
		JitOpFun* func_ptr          = nullptr;
		uint      until_compilation = 1;
	};

	/**
	 * @brief The data additionally stored by the JIT compiler
	 */
	using JitData = std::vector<JitFuncData>;

	vm::JitOpFun* compileJit(const vm::low::LowFuncData& func_data);
}
