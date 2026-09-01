#pragma once

#include <archiver/archive.hpp>
#include <driver/options.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>

#include <artifacts/artifacts.hpp>
#include <filesystem/file_path.hpp>
#include <string_id/string_id.hpp>

#include <json/diagnostics.hpp>
#include <nlohmann/json_fwd.hpp>


/**
 * @brief Defines the Task struct and related types for representing compilation tasks in the
 * Duckling compiler.
 */
MAKE_STRINGIFYABLE_ENUM(compiler::driver, u32, TaskType,
    PackageCompilation
)

namespace compiler::driver {
	/**
	 * @brief Diagnostics for task parsing and task resolution.
	 */
	namespace task {
		/**
		 * @brief Callback for reporting task diagnostics.
		 */
		using DiagnosticReporter = js::DiagnosticLogger;

		/**
		 * @brief Look up a package root module by its raw package id.
		 */
		base::Optional<compiler::frontend::ModuleID> getRootModuleIDForRawPackageId(
			base::StrID package_id, const DiagnosticReporter& report
		);
	}  // namespace task

	/**
	 * @brief Config/metadata that needs to be saved in the generated DBC code,
	 * in order to properly run the code.
	 */
	struct DVMLinkingOptions {
		/**
		 * @brief Shared libraries that have to be loaded to run the code.
		 */
		std::vector<std::string> shared_libraries{};

		/**
		 * @brief DBC libraries paths to link to final executable.
		 */
		std::vector<fs::File> link_libraries{};
	};

	/**
	 * @brief Package build target.
	 */
	struct BuildTargetDVMLibrary final {
		/**
		 * @brief The output file path name for the compiled DVM package.
		 */
		base::StrID output_file_name = base::StrID("package_dvm.dbc");

		/**
		 * @brief Runtime config of the DVM.
		 */
		DVMLinkingOptions dvm_linking_options = {};

		/**
		 * @brief If a value is present, use this artifact collection
		 * instead of the default.
		 * @note This only affects the compiled package output, not any
		 * other query artifacts (if present).
		 * @TODO: #3158 Solving this would remove the need for this option.
		 */
		base::Optional<Ref<artifacts::ArtifactCollection>> custom_art_collection = {};
	};

	struct BuildTargetDVMExecutable final {
		/**
		 * @brief The output file path name for the compiled DVM executable.
		 */
		base::StrID output_file_name;

		/**
		 * @brief Runtime config of the DVM.
		 * @note The standard library packages the executable depends on are added to
		 * `link_libraries` by `mergeBuildTargetWithGlobalOptions`.
		 */
		DVMLinkingOptions dvm_linking_options = {};
	};

	/**
	 * @brief Package build target for LLVM backend (to object files).
	 */
	struct BuildTargetLLVM final {};

	struct BuildTargetLLVMExecutable final {
		/**
		 * @brief The output file path for the compiled executable.
		 */
		base::StrID output_file_name;

		/**
		 * @brief Linking options for the executable.
		 */
		linker::LinkingOptions linking_options;
	};

	struct BuildTargetLLVMStaticLibrary final {
		/**
		 * @brief The output file path for the compiled static library.
		 */
		base::StrID output_file_name;

		/**
		 * @brief Archiving options for the static library.
		 */
		archiver::ArchivingOptions archiving_options;

		/**
		 * @brief If a value is present, use this artifact collection
		 * instead of the default.
		 * @note This only affects the compiled package output, not any
		 * other query artifacts (if present).
		 * @TODO: #3158 Solving this would remove the need for this option.
		 */
		base::Optional<Ref<artifacts::ArtifactCollection>> custom_art_collection = {};
	};

	/**
	 * @brief A variant type representing different build targets.
	 */
	using BuildTarget = std::variant<
		BuildTargetDVMLibrary,
		BuildTargetDVMExecutable,
		BuildTargetLLVM,
		BuildTargetLLVMExecutable,
		BuildTargetLLVMStaticLibrary>;

	/**
	 * @brief Represents a task for compiling a package
	 */
	struct RawPackageCompilationTask final {
		base::StrID package_id;
		BuildTarget build_target;

		/**
		 * @brief Parse a package compilation task from JSON.
		 */
		static base::Optional<RawPackageCompilationTask> fromJson(
			const nlohmann::json& json, const task::DiagnosticReporter& report
		);
	};

	struct PackageCompilationTask final {
		frontend::ModuleID root_module;
		BuildTarget        build_target;
	};

	/**
	 * @brief Represents a compilation task in the Duckling compiler.
	 * A task can be of different types, such as package compilation, and contains the relevant data
	 * for that task type.
	 */
	struct RawTask final {
		TaskType                                type;
		std::variant<RawPackageCompilationTask> task_data;

		/**
		 * @brief Parse a compilation task from JSON.
		 */
		static base::Optional<RawTask> fromJson(
			const nlohmann::json& json, const task::DiagnosticReporter& report
		);
	};

	struct Task final {
		TaskType                             type;
		std::variant<PackageCompilationTask> task_data;
	};

	/**
	 * @brief Convert a raw task into a resolved task.
	 * @note This function requires presence of the package specified in the task in global_state
	 */
	base::Optional<Task> convertRawTaskToTask(
		const RawTask&                      raw_task,
		const options_types::StdLibOptions& stdlib_options,
		const task::DiagnosticReporter&     report
	);

	/**
	 * @brief Construct linker options for a given task, based on the task's build target and the
	 * standard library options.
	 */
	linker::LinkingOptions constructNativeLinkerOptions(
		const options_types::LinkingOptions& linking_options,
		const options_types::StdLibOptions&  stdlib_options
	);

	/**
	 * @brief Construct DVM linking options for a given task, based on the task's build target and
	 * the standard library options.
	 * @param is_static_lib a static library does not link the standard library - that is left to
	 * the executable depending on it.
	 */
	DVMLinkingOptions constructDVMLinkingOptions(
		const options_types::LinkingOptions& linking_options,
		const options_types::StdLibOptions&  stdlib_options,
		bool                                 is_static_lib
	);
}
