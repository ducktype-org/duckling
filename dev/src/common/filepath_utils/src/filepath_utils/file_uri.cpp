#include "file_uri.hpp"

#include <base/config/target_info.hpp>

namespace filepath_utils {

	std::string formatFileUri(std::string_view path) {
		if constexpr (base::IS_TARGET_OS_WINDOWS)
			if (path.size() >= 2 && path[1] == ':') return "file:///" + std::string(path);
		return "file://" + std::string(path);
	}

}
