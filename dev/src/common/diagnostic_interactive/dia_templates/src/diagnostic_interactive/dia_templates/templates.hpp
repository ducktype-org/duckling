#pragma once

#include <optional>
#include <string_view>

namespace view_manager {
	std::optional<std::string_view> loadTemplateFromPath(std::string_view path);
}
