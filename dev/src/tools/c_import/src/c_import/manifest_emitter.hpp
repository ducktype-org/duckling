#pragma once

#include <string>
#include <vector>

namespace c_import {

	struct ManifestInput final {
		std::string package_name;
		std::string version;
		/** Passed to the linker verbatim, so it may hold several arguments. */
		std::string              links;
		std::vector<std::string> dvm_shared_libs;
	};

	struct ManifestResult final {
		std::string yaml;
		/** Empty when the input is usable. */
		std::string error;
	};

	ManifestResult emitManifest(const ManifestInput& input);

}
