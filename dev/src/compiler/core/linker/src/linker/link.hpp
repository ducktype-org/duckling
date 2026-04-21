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
		 * Path to the linker executable (e.g., "gcc" or "ld").
		 * If not set, the default linker will be used.
		 */
		base::Optional<std::string> linker_path;

		/**
		 * Additional options passed to the linker (e.g., "my_object.o" "-L/path/to/libs -lsomelib").
		 */
		std::string additional_link_options;

		/**
		 * @brief Whether to link the C standard library.
		 */
		bool link_c_standard_library;
	};

	/**
	 * @brief Link given object files into a single executable.
	 * In the future it will be changed to a query, to automatically support caching.
	 * @note we can add additional object/library files here when needed.
	 */
	[[nodiscard]]
	base::OkBad linkExecutable(
		const artifacts::FileArtifact&              output,
		const std::vector<artifacts::FileArtifact>& inputs,
		const LinkingOptions&                       options
	);

}
