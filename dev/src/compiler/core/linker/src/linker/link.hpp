#pragma once

#include <artifacts/artifacts.hpp>

#include <vector>

namespace compiler::linker {
	/**
	 * Linking options for external libraries and linker configuration.
	 * These are set during compiler initialization and used during linking phase.
	 */
	struct LinkingOptions final {
		/**
		 * Paths to external static libraries to link against.
		 */
		std::vector<fs::FilePath> external_static_libraries;

		/**
		 * @brief Whether to link the C standard library.
		 */
		bool link_c_standard_library;
	};

	/**
	 * Links given files (assumed to be object files) into a single executable file.
	 * In the future it will be changed to a query, to automatically support caching.
	 * @note: we can add additional object/library files here when needed.
	 */
	void link(
		const artifacts::FileArtifact& output, const std::vector<artifacts::FileArtifact>& inputs
	);

	/**
	 * @note This returns a const reference to prevent modification after initialization
	 */
	const LinkingOptions& getLinkingOptions();

	void setLinkingOptions(const LinkingOptions& options);

}
