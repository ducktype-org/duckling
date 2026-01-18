#pragma once

namespace compiler::driver {
	/**
	 * This is a debug option.
	 * If set, LLVM IR output will be dumped to a file in CompileModule query.
	 * It will not be placed in the output artifacts, rather it will be saved to a file in the
	 * current working directory.
	 */
	extern constinit bool llvm_dump_ir;

	/**
	 * This is a debug option.
	 * If set, Assembly output will be dumped to a file in CompileModule query.
	 * It will not be placed in the output artifacts, rather it will be saved to a file in the
	 * current working directory.
	 */
	extern constinit bool llvm_dump_asm;

	/**
	 * If set, incremental compilation is enabled.
	 * This flag is set during driver initialization based on user options.
	 * --no-incremental will disable it.
	 *
	 * @note This is currently set in initializeTheCompiler functions and used in driver::exit.
	 */
	extern constinit bool enable_incremental_compilation;
}
