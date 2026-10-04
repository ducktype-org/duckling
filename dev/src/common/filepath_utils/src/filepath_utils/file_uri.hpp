#pragma once

#include <string>
#include <string_view>

namespace filepath_utils {

	/**
	 * @brief Formats an absolute file path as a file:// URI.
	 *
	 * The path must be absolute. Relative paths are not representable in the
	 * file scheme (RFC 8089): "file://some/path" would be parsed as host
	 * "some" with path "/path", not as a relative path. Empty input is out of
	 * contract and yields the degenerate "file://" (behavior pinned by tests).
	 *
	 * @note On Windows, drive-letter paths (e.g. "C:/path") get the
	 *       empty-authority form "file:///C:/path". That branch is compiled
	 *       out on Linux/macOS and is not exercised by the unit tests until a
	 *       Windows CI runner is available. @TODO: #3343 Add Windows CI
	 *       coverage for os_utils/filepath_utils platform branches.
	 */
	std::string formatFileUri(std::string_view path);

}
