#pragma once

namespace compiler::driver {

	struct DumpIROptions {
		bool dump_llvm = false;
		bool dump_asm  = false;
		bool dump_lir  = false;
		bool dump_mir  = false;
		bool dump_hir  = false;
	};

	/**
	 * This is a debug option.
	 * If set, the selected IR output will be dumped to a file in the `duck_debug_artifacts`
	 * directory in the `compileModule` query.
	 */
	extern constinit DumpIROptions dump_ir_options;

	struct PrintIROptions {
		bool print_lir = false;
		bool print_mir = false;
		bool print_hir = false;
	};

	/**
	 * This is a debug option.
	 * If set, the compiler will print to output the selected IR
	 * in the `compileModule` query.
	 */
	extern constinit PrintIROptions print_ir_options;

	/**
	 * If set, incremental compilation is enabled.
	 * This flag is set during driver initialization based on user options.
	 * --no-incremental will disable it.
	 *
	 * @note This is currently set in initializeTheCompiler functions and used in driver::exit.
	 */
	extern constinit bool enable_incremental_compilation;
}
