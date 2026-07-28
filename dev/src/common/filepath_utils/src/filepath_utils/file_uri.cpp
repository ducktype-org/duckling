#include "file_uri.hpp"

namespace filepath_utils {

	std::string formatFileUri(std::string_view path) {
#ifdef _WIN32
		if (!path.empty() && path[1] == ':') return "file:///" + std::string(path);
#endif
		return "file://" + std::string(path);
	}

}
