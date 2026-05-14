#pragma once
#include <driver/task/task.hpp>
#include <frontend/packages/packages.hpp>

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <json/diagnostics.hpp>

#include <vector>

namespace compiler::driver {
	/**
	 * @brief Diagnostics for manifest parsing and validation.
	 */
	namespace manifest {
		/**
		 * @brief Callback for reporting manifest diagnostics.
		 */
		using DiagnosticReporter = js::DiagnosticLogger;
	}  // namespace manifest

	/**
	 * @brief Represents the manifest for package compilation in the Duckling compiler.
	 * The manifest contains information about the packages used in compilation processes and the
	 * tasks to be performed (like compile a single package).
	 */
	struct PackageCompilationManifest final {
		/**
		 * @brief A list of packages used in the compilation process.
		 * This includes the packages that are being compiled as well as their dependencies.
		 * Every dependency should have its own entry in this list, even if it's not a direct
		 * compilation target.
		 */
		std::vector<compiler::frontend::packages::RawPackageInfo> packages;

		/**
		 * @brief A list of tasks to be performed during compilation.
		 */
		std::vector<RawTask> tasks;

		/**
		 * @brief Parse a package compilation manifest from JSON.
		 */
		static base::Optional<PackageCompilationManifest> fromJson(
			const nlohmann::json& json, const manifest::DiagnosticReporter& report
		);

		/**
		 * @brief Verifies the integrity of the manifest.
		 * For example we need to check if every dependency of a package has a corresponding entry
		 * in the packages list
		 * @param report Reporter that receives any encountered diagnostics.
		 * @return base::OkBad indicating whether the manifest is valid.
		 */
		[[nodiscard]] base::OkBad verify(const manifest::DiagnosticReporter& report) const;
	};
}
