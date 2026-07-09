#include "file_path.hpp"

#include <filesystem_private/vfs.hpp>

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

namespace {
	Ref<fs::VFS> vfs = fs::VFS::getInstance();

	/**
	 * @brief Checks whether `path` lives inside the system's temporary directory.
	 *
	 * Strategy: canonicalize the temp directory once, then string-compare `path` against it —
	 * it matches if it *is* that directory or starts with it plus a separator. `path` need not
	 * exist. Since the comparison is textual, both sides must be in the same form: the temp dir
	 * is canonical, so a `path` given through a symlink would not match it (on macOS the temp dir
	 * sits under /var, a symlink to /private/var). If the direct check fails, retry once with the
	 * symlinks in `path` resolved via weakly_canonical (which tolerates non-existent paths).
	 */
	bool hasTemporaryPrefix(const std::filesystem::path& path) {
		auto temp_dir   = std::filesystem::canonical(std::filesystem::temp_directory_path());
		auto temp_exact = temp_dir.generic_string();
		// Prefix to match children of the temp dir; guarantee a trailing separator.
		auto temp_prefix = temp_exact;
		if (!temp_prefix.empty() && temp_prefix.back() != '/') temp_prefix += '/';

		auto matches = [&](const std::filesystem::path& p) {
			auto p_str = p.generic_string();
			return p_str == temp_exact || p_str.starts_with(temp_prefix);
		};

		if (matches(path)) return true;

		// Direct match failed: resolve symlinks in `path` and compare again (see strategy above).
		if (path.is_absolute()) {
			std::error_code ec;
			auto            resolved = std::filesystem::weakly_canonical(path, ec);
			if (!ec && matches(resolved)) return true;
		}

		return false;
	}
}

namespace fs {
	PathType FilePath::determinePathType(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path))
			return PathType::Virtual;
		else if (hasTemporaryPrefix(path))
			return PathType::Temporary;
		else if (!path.is_absolute())
			return PathType::Relative;
		else
			return PathType::Physical;
	}

	FilePath FilePath::canonical() const {
		if (type == PathType::Virtual) {
			// Virtual paths don't have canonical representation in the traditional sense
			CORE_PANIC("Cannot canonicalize virtual path: " + path.string());
		}
		return std::filesystem::canonical(path);
	}

	bool FilePath::isSymlink() const {
		if (type == PathType::Virtual) return false;  // Virtual paths are never symbolic links
		return std::filesystem::is_symlink(path);
	}

	FilePath FilePath::join(const FilePath& other) const {
		CORE_ASSERT(other.isRelative(), "Cannot join absolute path: " + other.path.string());
		return path / other.path;
	}

	FilePath FilePath::operator/(const FilePath& other) const { return join(other); }

	FilePath FilePath::toVirtualPath() const {
		if (type == PathType::Virtual) CORE_PANIC("Path is already virtual: " + path.string());
		if (type == PathType::Relative)
			CORE_PANIC(
				"Cannot convert relative path to virtual: " + path.string()
				+ " use absolute() first."
			);
		if (type != PathType::Physical)
			CORE_PANIC("Can only convert physical paths to virtual: " + path.string());

		auto root     = vfs->getRootPath();
		auto abs_path = std::filesystem::absolute(path);
		// Manually concatenate strings since operator/ ignores left side for absolute paths
		return root.generic_string() + abs_path.generic_string();
	}

	FilePath FilePath::toPhysicalPath() const {
		if (type != PathType::Virtual)
			CORE_PANIC("Can only convert virtual paths to physical: " + path.string());

		auto root     = vfs->getRootPath();
		auto path_str = path.generic_string();
		auto root_str = root.generic_string();

		// For exact root match
		if (path_str == root_str) return std::filesystem::path{};

		// Check if path starts with root and has separator after it
		if (path_str.starts_with(root_str)) {
			auto remaining = path_str.substr(root_str.size());
			// If remaining path doesn't start with '/', it means root didn't end with one
			// and we need to ensure we have the proper physical path
			if (!remaining.empty() && remaining[0] != '/')
				CORE_PANIC("Invalid virtual path format: " + path.string());
			// Return the remaining path (which should start with '/' for absolute paths)
			return remaining;
		}

		CORE_PANIC("Invalid virtual path: " + path.string());
	}

	FilePath FilePath::absolute() const {
		if (type != PathType::Relative) return *this;
		return std::filesystem::absolute(path);
	}

	bool FilePath::isAbsolute() const noexcept { return type != PathType::Relative; }

	bool FilePath::exists() const {
		if (type == PathType::Virtual)
			return vfs->exists(path);
		else
			return std::filesystem::exists(path);
	}

	std::string FilePath::uri() const {
		std::string p = path.generic_string();
#ifdef _WIN32
		if (!p.empty() && p[1] == ':') return "file:///" + p;
#endif
		return "file://" + p;
	}

	FilePath FilePath::getDefaultTempDirectoryPath() {
		static FilePath temp_directory = [] {
			FilePath f(std::filesystem::temp_directory_path());
			CORE_ASSERT(f.isTemporary(), "Default temp directory is not temporary");
			return f;
		}();
		return temp_directory;
	}

	FilePath FilePath::getDefaultVirtualDirectoryPath() {
		static FilePath virtual_directory_path(vfs->getRootPath());
		return virtual_directory_path;
	}
}
