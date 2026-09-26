#include "package_writer.hpp"

#include <fstream>
#include <system_error>

namespace c_import {

	namespace {

		std::string writeFile(const std::filesystem::path& path, const std::string& contents) {
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (!file) return "cannot open " + path.string() + " for writing";
			file << contents;
			if (!file) return "cannot write " + path.string();
			return {};
		}

	}

	WriteResult writePackage(
		const std::filesystem::path& out_dir, const PackageLayout& layout, bool force
	) {
		std::error_code error_code;

		if (std::filesystem::exists(out_dir, error_code)
		    && !std::filesystem::is_empty(out_dir, error_code) && !force)
			return { out_dir.string() + " is not empty, pass --force to overwrite" };

		const std::filesystem::path source_dir = out_dir / "src";
		std::filesystem::create_directories(source_dir, error_code);
		if (error_code)
			return { "cannot create " + source_dir.string() + ": " + error_code.message() };

		if (auto error = writeFile(out_dir / "quackconfig.yaml", layout.manifest_yaml);
		    !error.empty())
			return { error };

		if (auto error = writeFile(source_dir / "src.dk", layout.root_module); !error.empty())
			return { error };

		for (const auto& module: layout.modules)
			if (auto error = writeFile(source_dir / (module.name + ".dk"), module.contents);
			    !error.empty())
				return { error };

		return {};
	}

}
