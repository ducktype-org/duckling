#pragma once

#include <frontend/pst_parser/pst.hpp>

#include <base/types/ok_bad.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {
	fs::File createFileFromVirtualRoot(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	);

	fs::File getFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path);

	void updateFileContent(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	);
}
