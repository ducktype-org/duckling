#pragma once

#include <frontend/packages/packages.hpp>
#include <global_state/backend_options.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>

#include <string>
#include <variant>

namespace compiler::driver {
	/**
	 * Definition of options that are used by the compiler to control its behavior.
	 */
	namespace options_types {
		struct DependencyInfo;

		/**
		 * Options used to control debug related behavior like
		 * logging, dump of intermediate representations, etc.
		 */
		struct DebugOptions final {
			// options mapping to logger categories:
			std::vector<std::string> dev_log_categories;
			bool                     immediate_print_diagnostics = true;

			// Debug dumping to file options
			bool dump_llvm = false;
			bool dump_asm  = false;
			bool dump_lir  = false;
			bool dump_mir  = false;
			bool dump_hir  = false;

			// Debug printing to stdout options
			bool print_lir = false;
			bool print_mir = false;
			bool print_hir = false;
		};

		/**
		 * Options related to incremental compilation.
		 * @param enabled Whether incremental compilation is enabled.
		 */
		struct IncrementalOptions final {
			bool enabled = true;
		};

		/**
		 * Options related to the execution management of the compiler.
		 */
		struct ExecutionOptions final {
			/**
			 * Number of workers to use for concurrent tasks run on the worker manager (mainly for
			 * query).
			 */
			u64 worker_count = 1;
		};

		struct ArtifactsOptions final {
			fs::FilePath artifacts_path;

			// bool rm_artifacts_before_compilation = false;
			// bool rm_artifacts_after_compilation = false;
		};

		struct StdLibOptions final {
			struct NoStd final {};

			struct DefaultStd final {};

			struct CustomStd final {
				fs::FilePath std_path;
			};

			/**
			 * The type of standard library to use, or
			 * no standard library at all.
			 */
			std::variant<NoStd, DefaultStd, CustomStd> std_lib_type = NoStd{};

			/**
			 * If a value is present, do not recompile the standard library
			 * and try to use the compiled packages from this path.
			 * @note The path should point to valid compiler artifacts location
			 */
			base::Optional<fs::FilePath> std_artifacts_path = {};

			[[nodiscard]] bool stdActive() const { return not base::holds<NoStd>(std_lib_type); }
		};

		struct LinkingOptions final {
			/**
			 * Options only supported on Native backend.
			 */

			/**
			 * @brief Name of the linker to use.
			 */
			base::Optional<std::string> native_linker_path;
			/**
			 * @brief The options that will be passed "as-is" to linker.
			 */
			base::Optional<std::string> native_additional_link_options;
			/**
			 * @brief Whether to link the c standard library.
			 */
			bool native_link_c_standard_lib = true;

			/**
			 * @brief The libraries that needs to be loaded by the VM to run the code.
			 */
			std::vector<std::string> dvm_shared_libraries;

			/**
			 * Options only supported on DVM backend.
			 */
			// Empty for now...
		};
	}

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
			std::vector<compiler::frontend::packages::RawPackageInfo> packages_info;
			options_types::ArtifactsOptions                           compilation_artifacts;
			global_state::BackendOptions                              backend_options;
			options_types::DebugOptions                               debug_options;
			options_types::IncrementalOptions                         incremental;
			options_types::ExecutionOptions                           execution_options;
			options_types::StdLibOptions                              stdlib_options;
		};

		/**
		 * Repl mode does not create a main package, does not enable incremental compilation,
		 * and does not persist artifacts to disk.
		 */
		struct ReplMode final {
			options_types::DebugOptions     debug_options;
			options_types::ExecutionOptions execution_options;
			options_types::StdLibOptions    stdlib_options;
		};

		/**
		 * Script compilation mode for .ds files.
		 *
		 * Compiles a script top-to-bottom (like REPL statements executed in sequence),
		 * but produces a single persistent artifact (.dbc or native executable)
		 * instead of executing immediately.
		 *
		 * Like ReplMode, does not set up a main package or incremental compilation.
		 * The script file is extracted to global_state::ScriptContext during initialization.
		 */
		struct ScriptMode final {
			fs::File                        script_file;
			global_state::BackendOptions    backend_options;
			options_types::ArtifactsOptions compilation_artifacts;
			options_types::DebugOptions     debug_options;
			options_types::ExecutionOptions execution_options;
			options_types::StdLibOptions    stdlib_options;
		};

		/**
		 * @note: in the future this might hold more modes,
		 * like lsp daemon, etc.
		 * don't refrain from refactoring this file (and module) if needed.
		 * We might also want to restrain compiler functionality based on the mode.
		 */
		std::variant<BareMode, PackageCompilationMode, ReplMode, ScriptMode> mode;

		CompilerModeOfOperationAndOptions(BareMode bare_mode): mode(bare_mode) {}

		CompilerModeOfOperationAndOptions(PackageCompilationMode package_mode):
			  mode(package_mode) {}

		CompilerModeOfOperationAndOptions(ReplMode repl_mode): mode(repl_mode) {}

		CompilerModeOfOperationAndOptions(ScriptMode script_mode): mode(script_mode) {}
	};
};
