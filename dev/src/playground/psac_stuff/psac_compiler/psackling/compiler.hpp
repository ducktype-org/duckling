#pragma once

#include <filesystem/file.hpp>

namespace pg {
	void compile(const fs::File& file, std::string_view output_path = "psacklinged.out");
}
