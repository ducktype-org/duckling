#pragma once

#include "options.hpp"

#include <base/collections/optional.hpp>

#include <filesystem/file_path.hpp>

#include <vector>

namespace compiler::driver {

	/**
	 * @brief Load external dependencies from a JSON manifest file.
	 *
	 * Expected schema:
	 * @code
	 * {
	 *   "externs": [
	 *     {
	 *       "name": "std",
	 *       "source": "./pkgs/std",
	 *       "strategy": { "type": "precompiled" }
	 *     }
	 *   ]
	 * }
	 * @endcode
	 *
	 * Valid `strategy.type` values: "precompiled", "inline".
	 *
	 * On any error (missing file, malformed JSON, missing/invalid field, duplicate
	 * names) logs a diagnostic to the global logger and returns an empty Optional.
	 */
	[[nodiscard]]
	base::Optional<std::vector<options_types::DependencyInfo>> loadExternsManifest(const fs::FilePath& manifest_path);
}
