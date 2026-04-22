/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <base/pointers/ref.hpp>

#include <vm/core/safe/low_program/low_program.hpp>

#ifdef COMPILATION_THRESHOLD
constexpr inline uint compilation_threshold = COMPILATION_THRESHOLD;
#else
constexpr inline uint compilation_threshold = 1;
#endif

namespace vm::jit {
	using JitOpFun
		= void(const vm::MicroInstruction**, std::byte**, vm::Frame**, vm::SafeVMThread*);

	/**
	 * @brief The data additionally stored per function, by the JIT compiler.
	 */
	struct JitFuncData {
		MRef<JitOpFun> func_ptr          = nullptr;
		uint           until_compilation = compilation_threshold;
	};

	/**
	 * @brief The data additionally stored by the JIT compiler.
	 */
	using JitData = std::vector<JitFuncData>;

	/**
	 * @brief Compile the function on the C2, LLVM-based compiler.
	 */
	MRef<JitOpFun> compileLLVM(const vm::low::LowFuncData& func_data);
}
