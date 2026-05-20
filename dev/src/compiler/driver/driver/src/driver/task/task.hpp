#pragma once

#include <archiver/archive.hpp>
#include <driver/options.hpp>
#include <frontend/module_tree/module_id.hpp>
#include <global_state/packages.hpp>
#include <linker/link.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>

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
		 * @brief Look up a package root module by its raw package name.
		 */
		base::Optional<compiler::frontend::ModuleID> getRootModuleIDForRawPackageName(
			base::StrID package_name, const DiagnosticReporter& report
		);
	}  // namespace task

	/**
	 * @brief Package build target.
	 */
	struct BuildTargetDVM {
		/**
		 * @brief The output file path stem for the compiled DVM package.
		 */
		base::StrID output_file_stem = base::StrID("package_dvm");
	};

	/**
	 * @brief Package build target for LLVM backend (to object files).
	 */
	struct BuildTargetLLVM {};

	struct BuildTargetLLVMExecutable {
		/**
		 * @brief The output file path for the compiled executable.
		 */
		base::StrID output_file_stem;

		/**
		 * @brief Linking options for the executable.
		 */
		linker::LinkingOptions linking_options;
	};

	struct BuildTargetLLVMStaticLibrary {
		/**
		 * @brief The output file path for the compiled static library.
		 */
		base::StrID output_file_stem;

		/**
		 * @brief Archiving options for the static library.
		 */
		archiver::ArchivingOptions archiving_options;
	};

	/**
	 * @brief A variant type representing different build targets.
	 */
	using BuildTarget = std::variant<
		BuildTargetDVM,
		BuildTargetLLVM,
		BuildTargetLLVMExecutable,
		BuildTargetLLVMStaticLibrary>;

	/**
	 * @brief Represents a task for compiling a package
	 */
	struct RawPackageCompilationTask final {
		base::StrID package_name;
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
	struct RawTask {
		TaskType                                type;
		std::variant<RawPackageCompilationTask> task_data;

		/**
		 * @brief Parse a compilation task from JSON.
		 */
		static base::Optional<RawTask> fromJson(
			const nlohmann::json& json, const task::DiagnosticReporter& report
		);
	};

	struct Task {
		TaskType                             type;
		std::variant<PackageCompilationTask> task_data;
	};

	/**
	 * @brief Convert a raw task into a resolved task.
	 * @note This function requires presence of the package specified in the task in global_state
	 */
	base::Optional<Task> convertRawTaskToTask(
		const RawTask&                             raw_task,
		const options_types::GlobalLinkingOptions& global_linking_options,
		const task::DiagnosticReporter&            report
	);

	/**
	 * @brief Set global linking options to linker::LinkingOptions.
	 */
	linker::LinkingOptions mergeBothLinkingOptions(
		const linker::LinkingOptions&              local_options,
		const options_types::GlobalLinkingOptions& global_linking_options
	);
}
