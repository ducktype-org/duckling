#pragma once

#include <nlohmann/json_fwd.hpp>
#include <global_state/packages.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include "filesystem/file_path.hpp"
#include "string_id/string_id.hpp"


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
		fs::FilePath            output;

        /**
         * @brief Linking options passed by a string to the linker.
         */
		base::StrID             additional_linking_options;
	};

	struct BuildTargetLLVMStaticLibrary {
		fs::FilePath                output;
	};

	using BuildTarget
		= std::variant<BuildTargetDVM, BuildTargetLLVM, BuildTargetLLVMExecutable, BuildTargetLLVMStaticLibrary>;

    struct PackageCompilationTask final {
        base::StrID package_name;
        BuildTarget build_target;

        static base::Optional<PackageCompilationTask> fromJson(const nlohmann::json& json);
    };

    struct Task {
        TaskType type;
        std::variant<PackageCompilationTask> task_data;

        static base::Optional<Task> fromJson(const nlohmann::json& json);
    };
}