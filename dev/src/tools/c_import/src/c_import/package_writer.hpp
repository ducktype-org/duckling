#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace c_import {

	struct PackageFile final {
		/** File stem; the `.dk` extension is added when writing. */
		std::string name;
		std::string contents;
	};

	struct PackageLayout final {
		std::string package_name;
		std::string manifest_yaml;
		std::string root_module;
		/** One entry when flat, one per translated header when splitting. */
		std::vector<PackageFile> modules;
	};

	struct WriteResult final {
		/** Empty on success. */
		std::string error;
	};

	/**
	 * @brief Writes the package to @p out_dir.
	 *
	 * Refuses to touch a non-empty directory unless @p force is set.
	 */
	WriteResult writePackage(
		const std::filesystem::path& out_dir, const PackageLayout& layout, bool force
	);

}
