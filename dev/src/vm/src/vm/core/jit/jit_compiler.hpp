/**
 * @file jit_compiler.hpp
 * @brief The JIT compiler API for micro-instructions.
 * @details Breaks the dependency on LLVM.
 */
#pragma once

#include <base/pointers/ref.hpp>

#include <vm/core/jit/copy-and-patch/memory/memory.hpp>
#include <vm/core/safe/low_program/cfg/cf_graph.hpp>
#include <vm/core/safe/low_program/cfg/loop_detector.hpp>

#ifdef BUILD_TYPE_RELEASE
constexpr inline uint LLVM_FUNC_COMPILATION_THRESHOLD = 10'000;
constexpr inline uint LOOP_COMPILATION_THRESHOLD      = 10'000;
#else
// During testing compile always to check properly that jit integration works.
constexpr inline uint LLVM_FUNC_COMPILATION_THRESHOLD = 0;
constexpr inline uint LOOP_COMPILATION_THRESHOLD      = 0;
#endif

#if COMPILE_WITH_CNP
constexpr inline uint CP_FUNC_COMPILATION_THRESHOLD = 0;
#endif


namespace vm {
	struct Frame;
	class SafeVMThread;
}

namespace vm::jit {
	// There is a strong dependency in creating this type for LLVM. (jit_data.cpp)
	using JitLLVMFunc = i64(const vm::MicroInstruction**, byte**, vm::Frame**, vm::SafeVMThread*);


#define CP_RETURN __attribute__((preserve_none)) void
#define CP_ARGS                                                                \
	[[maybe_unused]] ::byte *local_stack, [[maybe_unused]] ::vm::Frame *frame, \
		[[maybe_unused]] ::vm::SafeVMThread &thread
#define CP_PASS_ARGS local_stack, frame, thread
	using JitCPFunc = CP_RETURN(CP_ARGS);

	/**
	 * @brief The data additionally stored per function, by the JIT compiler.
	 * @details The vectors below are indexed by instruction offset within the function's
	 * bytecode (size == number of instructions), not by basic block. Only loop headers and
	 * the function entrypoint carry meaningful values; other slots stay empty/default.
	 */
	struct JitFuncData final {
		// CFG of the loop for each loop header; full-function CFG at the entrypoint offset.
		std::vector<low::cf::ControlFlowGraph> cfgs;
		// Executions left before the loop/function starting at this offset gets compiled.
		std::vector<uint>              until_compilation;
		std::vector<MRef<JitLLVMFunc>> llvm_compiled_code_ptrs;
#if COMPILE_WITH_CNP
		std::optional<cnp::JitFuncMemory> cp_memory = std::nullopt;
#endif

		JitFuncData() = default;

		JitFuncData(const low::MicroBytecode& bc, usize entrypoint_offset):
			  cfgs(low::cf::LoopDetector::detectLoopsInFunction(bc, entrypoint_offset)),
			  until_compilation(cfgs.size(), LOOP_COMPILATION_THRESHOLD),
			  llvm_compiled_code_ptrs(cfgs.size(), nullptr) {
#if COMPILE_WITH_CNP
			until_compilation[entrypoint_offset] = CP_FUNC_COMPILATION_THRESHOLD;
#else
			until_compilation[entrypoint_offset] = LLVM_FUNC_COMPILATION_THRESHOLD;
#endif
		}
	};

	/**
	 * @brief Compile the contiguous bytecode block (function or loop) on the C2, LLVM-based compiler.
	 * @param cfg Control flow graph of the block to be compiled.
	 * @param bc Bytecode of the compiled bytecode block.
	 * @param name Identifier of the compiled block.
	 */
	MRef<JitLLVMFunc> compileLLVM(
		const vm::low::cf::ControlFlowGraph& cfg,
		const vm::low::MicroBytecode&        bc,
		const base::StrID&                   name
	);

	/**
	 * @brief Compile the function on the C1, Copy&Patch-based compiler.
	 */
	std::expected<cnp::JitFuncMemory, std::string> compileCP(
		const vm::low::cf::ControlFlowGraph& cfg, const vm::low::MicroBytecode& bc
	);
}
