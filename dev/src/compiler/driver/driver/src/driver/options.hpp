#pragma once

#include <base/ints.hpp>
#include <base/optional.hpp>
#include <filesystem/file.hpp>

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
		struct CompilationOptions {
			// struct OptimizationOptions {
			//     u64 level;
			// };

			struct BackendOptions final {
				struct VMBackend {};

				struct LLVMBackend {};

				// /**
				//  * If empty, then VMBackend is not available.
				//  */
				// base::Optional<VMBackend> vm_backend;

				// /**
				//  * If empty, then LLVMBackend is not available.
				//  */
				// base::Optional<LLVMBackend> llvm_backend;
			};

			CompilationOptions(BackendOptions backend): backend(backend) {}

			// OptimizationOptions optimization;
			BackendOptions backend;
		};

		struct DebugOptions final {
			bool lexer_cerr;
			bool logger_cerr;
		};

		struct ArtifactsOptions final {
			fs::File artifacts_path;

			// struct IncrementalCompilation {
			//     bool enabled;
			//     bool show_stats;
			// };
			// IncrementalCompilation incremental_compilation;
			// bool rm_artifacts_before_compilation = false;
			// bool rm_artifacts_after_compilation = false;
		};

		struct PackageInfo {
			std::string package_name;
			std::string package_path;
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
	 */
	struct CompilerModeOfOperationAndOptions {
		/**
		 * Bare mode, where compiler doesn't do any initializations apart from debug options,
		 * but you can still (try to) use its internal functions by hand.
		 */
		struct BareMode {
			options_types::DebugOptions debug_options;
		};

		/**
		 * Package compilation mode, compiler is used to compile a package
		 * and its dependencies.
		 */
		struct PackageCompilationMode {
			// options_types::PackageInfo      main_package_info;
			options_types::ArtifactsOptions compilation_artifacts;
			// std::vector<options_types::DependencyInfo> dependencies;
			// options_types::CompilationOptions compilation_options;
			options_types::DebugOptions       debug_options;
		};

		/**
		 * @note: in the future this might hold more modes,
		 * like repl mode, script compilation mode, lsp deamon, etc.
		 * don't refrain from refactoring this file (and module) if needed.
		 */
		std::variant<BareMode, PackageCompilationMode> mode;

		CompilerModeOfOperationAndOptions(BareMode bare_mode)
			: mode(bare_mode) {}

		CompilerModeOfOperationAndOptions(PackageCompilationMode package_mode)
			: mode(package_mode) {}
	};
};
