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
}
