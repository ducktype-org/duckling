#pragma once

#include <optional>
#include <string_view>

namespace dia_embedded {
	std::optional<std::string_view> loadTemplateFromPath(std::string_view path);
}
