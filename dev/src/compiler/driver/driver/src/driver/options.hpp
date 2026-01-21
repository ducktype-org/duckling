#pragma once

#include <linker/link.hpp>

#include <base/collections/optional.hpp>
#include <base/types/ints.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>

#include <string>
#include <variant>

namespace compiler::driver {
	// @note: a lot of code in this file is left as hypothetical comments
	// as it is unused for now, but sets a vision for the code structure
	// in the future.
	// I'm not 100% sure if this place is the best place for this code,
	// as in the end those options should be accessible (via global_state module of other things)
	// in the core-compiler, and we don't want to have a dependency on the driver module there.

	/**
	 * Definition of options that are used by the compiler to control its behavior.
	 */
	namespace options_types {
		/**
		 * Options used for actual compilation of the source code.
		 */
		struct CompilationOptions final {
			// struct OptimizationOptions {
			//     u64 level;
			// };

			struct BackendOptions final {
				struct DVMBackend {};

				struct LLVMBackend {};

				// /**
				//  * If empty, then DVM backend is not available.
				//  */
				// base::Optional<VMBackend> dvm_backend;

				// /**
				//  * If empty, then LLVM backend is not available.
				//  */
				// base::Optional<LLVMBackend> llvm_backend;
			};

			CompilationOptions(BackendOptions backend): backend(backend) {}

			// OptimizationOptions optimization;
			BackendOptions backend;
		};

		/**
		 * Options used to control debug related behavior like
		 * logging, dump of intermediate representations, etc.
		 */
		struct DebugOptions final {
			// options mapping to logger categories:
			std::vector<std::string> dev_log_categories;
			bool                     immediate_print_diagnostics = true;
			// options mapping to driver module flags:
			bool dump_llvm_ir  = false;
			bool dump_llvm_asm = false;
		};

		/**
		 * Options related to incremental compilation.
		 * @param enabled Whether incremental compilation is enabled.
		 */
		struct IncrementalOptions final {
			bool enabled = true;
		};

		struct ArtifactsOptions final {
			fs::FilePath artifacts_path;

			// struct IncrementalCompilation {
			//     bool enabled;
			//     bool show_stats;
			// };
			// IncrementalCompilation incremental_compilation;
			// bool rm_artifacts_before_compilation = false;
			// bool rm_artifacts_after_compilation = false;
		};

		struct PackageInfo final {
			std::string  package_name;
			fs::FilePath package_path;
		};

		// struct DependencyInfo {
		//     struct CompilationStrategy {
		//         struct InlineCompilation { };
		//         struct Precompiled {
		//             // this might be inlined or not:
		//             std::string precompilation_path;
		//         };
		//         std::variant<InlineCompilation, Precompiled> strategy;
		//     };
		//     PackageInfo package_info;
		//     CompilationStrategy compilation_strategy;
		// };
	};

	/**
	 * Structure holding information about mode of operation of the compiler
	 * and all "generic" compiler options associated with that mode,
	 * (i.e. options that are not associated with any specific task).
	 * Note that is doesn't encapsulate cli options such as --help or --version.
	 * It is used to configure the compiler's behavior once the compiler is actually used
	 * via the Driver.
	 * @important: this structure is used only to initialize the compiler, not to store the options
	 * themself.
	 */
	struct CompilerModeOfOperationAndOptions final {
		/**
		 * Bare mode, where compiler doesn't do any initializations apart from debug options,
		 * but you can still (try to) use its internal functions by hand.
		 */
		struct BareMode final {
			options_types::DebugOptions debug_options;
		};

		/**
		 * Package compilation mode, compiler is used to compile a package
		 * and its dependencies.
		 */
		struct PackageCompilationMode final {
			options_types::PackageInfo      main_package_info;
			options_types::ArtifactsOptions compilation_artifacts;
			// std::vector<options_types::DependencyInfo> dependencies;
			// options_types::CompilationOptions compilation_options;
			options_types::DebugOptions       debug_options;
			options_types::IncrementalOptions incremental;
		};

		/**
		 * Repl mode does not create a main package, does not enable incremental compilation,
		 * and does not persist artifacts to disk.
		 */
		struct ReplMode final {
			options_types::DebugOptions debug_options;
		};

		/**
		 * @note: in the future this might hold more modes,
		 * like script compilation mode, lsp deamon, etc.
		 * don't refrain from refactoring this file (and module) if needed.
		 * We might also want to restrain compiler functionality based on the mode.
		 */
		std::variant<BareMode, PackageCompilationMode, ReplMode> mode;

		CompilerModeOfOperationAndOptions(BareMode bare_mode): mode(bare_mode) {}

		CompilerModeOfOperationAndOptions(PackageCompilationMode package_mode):
			  mode(package_mode) {}

		CompilerModeOfOperationAndOptions(ReplMode repl_mode): mode(repl_mode) {}
	};
};
