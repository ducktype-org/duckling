/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <base/pointers/ref.hpp>

#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/cfg/loop_detector.hpp>
#include <vm/core/safe/low_program/low_program.hpp>

#ifdef BUILD_TYPE_RELEASE
constexpr inline uint FUNC_COMPILATION_THRESHOLD = 10;
constexpr inline uint LOOP_COMPILATION_THRESHOLD = 10;
#else
// During testing compile always to check properly that jit integration works.
constexpr inline uint FUNC_COMPILATION_THRESHOLD = 10;
constexpr inline uint LOOP_COMPILATION_THRESHOLD = 10;
#endif

namespace vm::jit {
	using JitOpFun
		= i64(const vm::MicroInstruction**, std::byte**, vm::Frame**, vm::SafeVMThread*);

	/**
	 * @brief The data additionally stored per function, by the JIT compiler.
	 */
	struct JitFuncData {
		std::vector<low::cf::ControlFlowGraph> cfgs;
		std::vector<uint>                      until_compilation;
		std::vector<MRef<JitOpFun>>            compiled_code_ptrs;

		JitFuncData() = default;
		JitFuncData(const low::LowFuncData& func):
			cfgs(low::cf::detectLoopsInFunction(func)),
			until_compilation(cfgs.size(), LOOP_COMPILATION_THRESHOLD),
			compiled_code_ptrs(cfgs.size(), nullptr) {
				until_compilation[func.jit_entrypoint_offset] = FUNC_COMPILATION_THRESHOLD;
			}
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
}
