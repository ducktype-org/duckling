#pragma once

#include <string>
#include <string_view>

namespace filepath_utils {

	/// Formats a file path as a file:// URI, handling Windows drive letters.
	std::string formatFileUri(std::string_view path);

}
