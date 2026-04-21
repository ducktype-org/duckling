#pragma once

#include "backend_type.hpp"
#include "options.hpp"

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {

	struct PackageCompilationManifestEntry final {
		options_types::PackageInfo package_info;
		std::string                output_file_name;
		BuildTarget                build_target;
	};

	/**
	 * @brief Load package compilation requests from a JSON manifest file.
	 *
	 * Expected schema:
	 * @code
	 * {
	 *   "packages": [
	 *     {
	 *       "name": "app",
	 *       "source": "./pkgs/app",
	 *       "output-file-name": "app_bin",
	 *       "build-target": {
	 *         "type": "llvm-executable",
	 *         "additional-link-options": "-lm"
	 *       },
	 *       "dependencies": [
	 *         {
	 *           "name": "std",
	 *           "source": "./pkgs/std",
	 *           "strategy": { "type": "precompiled" }
	 *         }
	 *       ]
	 *     }
	 *   ]
	 * }
	 * @endcode
	 *
	 * Valid dependency `strategy.type` values: "precompiled", "inline".
	 *
	 * Valid `build-target.type` values: "dvm", "llvm-executable",
	 * "llvm-static-library".
	 *
	 * On any error (missing file, malformed JSON, missing/invalid field, duplicate
	 * names) logs a diagnostic to the global logger and returns an empty Optional.
	 */
	[[nodiscard]]
	base::Optional<std::vector<PackageCompilationManifestEntry>> loadPackagesManifest(
		const fs::FilePath& manifest_path
	);
}
