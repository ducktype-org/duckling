/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <base/pointers/ref.hpp>

#include <vm/core/jit/copy-and-patch/memory/memory.hpp>
#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

// During testing compile always to check properly that jit integration works.
constexpr inline uint COMPILATION_THRESHOLD = 0;

namespace vm::jit {
	using JitOpFun
		= void(const vm::MicroInstruction**, std::byte**, vm::Frame**, vm::SafeVMThread*);

#define CP_ARGS const MicroInstruction *instr, byte *local_stack, Frame *frame, SafeVMThread &thread
	using CPFunc = void(CP_ARGS);

	/**
	 * @brief The data additionally stored per function, by the JIT compiler.
	 */
	struct JitFuncData {
	#if COMPILE_WITH_CP
		MRef<CPFunc> func_ptr          = nullptr;
	#else
		MRef<JitOpFun> func_ptr          = nullptr;
	#endif
		uint           until_compilation = COMPILATION_THRESHOLD;
	};

	/**
	 * @brief The data additionally stored by the JIT compiler.
	 */
	using JitData = std::vector<JitFuncData>;

	/**
	 * @brief Compile the contiguous bytecode block (function or loop) on the C2, LLVM-based compiler.
	 * @param cfg Control flow graph of the block to be compiled.
	 * @param bc Bytecode of the compiled bytecode block.
	 * @param name Identifier of the compiled block.
	 */
	MRef<JitOpFun> compileLLVM(
		const vm::low::cf::ControlFlowGraph& cfg,
		const vm::low::MicroBytecode&        bc,
		const base::StrID&                   name
	);

	/**
	 * @brief Compile the function on the C1, Copy&Patch-based compiler.
	 */
	cnp::JitFuncMemory compileCP(
		const vm::low::cf::ControlFlowGraph& cfg, const vm::low::MicroBytecode& bc
	);
}
