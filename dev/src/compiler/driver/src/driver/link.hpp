#pragma once

#include <artifacts/artifacts.hpp>

#include <vector>

namespace compiler::driver {
	struct LinkOptions final {
		bool link_c_standard_library = true;  // Whether to link the C standard library.
	};

	/**
	 * Links given files (assumed to be object files) into a single executable file.
	 * In the future it will be changed to a query, to automatically support caching.
	 */
	void link(
		artifacts::FileArtifact              output,
		std::vector<artifacts::FileArtifact> inputs,
		LinkOptions                          options
	);
}
