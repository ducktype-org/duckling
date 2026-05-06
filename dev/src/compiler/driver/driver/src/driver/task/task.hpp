#pragma once

#include <nlohmann/json_fwd.hpp>
#include <global_state/packages.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include "filesystem/file_path.hpp"
#include "string_id/string_id.hpp"
#include "frontend/module_tree/module_id.hpp"


/**
 * @brief Defines the Task struct and related types for representing compilation tasks in the Duckling compiler.
 */
MAKE_STRINGIFYABLE_ENUM(compiler::driver, u32, TaskType,
    PackageCompilation
)

namespace compiler::driver {
    /**
	 * @brief Package build target.
	 */
	struct BuildTargetDVM {};

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
         * @brief Linking options passed by a string to the linker.
         */
		base::StrID             additional_linking_options;
	};

	struct BuildTargetLLVMStaticLibrary {
        /**
         * @brief The output file path for the compiled static library.
         */
		base::StrID                output_file_stem;
	};

    /**
     * @brief A variant type representing different build targets.
     */
	using BuildTarget
		= std::variant<BuildTargetDVM, BuildTargetLLVM, BuildTargetLLVMExecutable, BuildTargetLLVMStaticLibrary>;

    /**
     * @brief Represents a task for compiling a package
     */
    struct RawPackageCompilationTask final {
        base::StrID package_name;
        BuildTarget build_target;

        static base::Optional<RawPackageCompilationTask> fromJson(const nlohmann::json& json);
    };

    struct PackageCompilationTask final {
        frontend::ModuleID root_module;
        BuildTarget build_target;
    };


    /**
     * @brief Represents a compilation task in the Duckling compiler.
     * A task can be of different types, such as package compilation, and contains the relevant data for that task type.
     */
    struct RawTask {    
        TaskType type;
        std::variant<RawPackageCompilationTask> task_data;

        static base::Optional<RawTask> fromJson(const nlohmann::json& json);
    };

    struct Task {
        TaskType type;
        std::variant<PackageCompilationTask> task_data;
    };

    Task convertRawTaskToTask(const RawTask& raw_task);
}