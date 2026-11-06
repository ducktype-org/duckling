// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file_path.hpp"

#include <base/except/exceptions.hpp>
#include <base/pointers/ref.hpp>

#include <filepath_utils/file_uri.hpp>
#include <filesystem/vfs.hpp>

namespace {
	/**
	 * @brief Checks whether `path` lives inside the system's temporary directory.
	 *
	 * The check is textual: `path` (which may not exist) matches if it equals the canonical
	 * temp directory or starts with it plus a separator. A path that reaches the temp dir
	 * through a symlink (on macOS the temp dir sits under /var -> /private/var) cannot match
	 * textually, so it is retried once with its own symlinks resolved.
	 */
	bool hasTemporaryPrefix(const std::filesystem::path& path) {
		// Only absolute paths can point into the temp directory.
		if (!path.is_absolute()) return false;

		auto temp_dir
			= std::filesystem::canonical(std::filesystem::temp_directory_path()).generic_string();
		auto is_in_temp_dir = [&temp_dir](const std::filesystem::path& p) {
			auto str = p.generic_string();
			if (!str.starts_with(temp_dir)) return false;
			// Reject siblings like `/tmpfoo`: the prefix must end exactly at a separator.
			return str.size() == temp_dir.size() || str[temp_dir.size()] == '/';
		};

		if (is_in_temp_dir(path)) return true;

		// Retry with symlinks resolved; weakly_canonical tolerates non-existent paths.
		std::error_code ec;
		auto            resolved = std::filesystem::weakly_canonical(path, ec);
		return !ec && is_in_temp_dir(resolved);
	}
}

namespace fs {
	base::Optional<base::Ref<VFS>> FilePath::defaultVfsFor(PathType type) {
		if (type == PathType::Virtual) return VFS::getInstance();
		return {};
	}

	PathType FilePath::determinePathType(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path))
			return PathType::Virtual;
		else if (!path.is_absolute())
			return PathType::Relative;
		else
			return PathType::Physical;
	}

	std::expected<FilePath, std::string> FilePath::canonical() const {
		if (type == PathType::Virtual) {
			// Virtual paths don't have canonical representation in the traditional sense
			CORE_PANIC("Cannot canonicalize virtual path: " + path.string());
		}
		std::error_code ec;
		auto            resolved = std::filesystem::canonical(path, ec);
		if (ec) return std::unexpected(path.string() + ": " + ec.message());
		return FilePath(std::move(resolved));
	}

	bool FilePath::isTemporary() const {
		if (type == PathType::Virtual) return false;
		// A relative path is resolved against the working directory first: whether the file
		// ends up in the temp directory is a question about the file, not about how the path
		// happened to be spelled.
		if (type == PathType::Relative) return hasTemporaryPrefix(std::filesystem::absolute(path));
		return hasTemporaryPrefix(path);
	}

	bool FilePath::isSymlink() const {
		if (type == PathType::Virtual) return false;  // Virtual paths are never symbolic links
		std::error_code ec;
		return std::filesystem::is_symlink(std::filesystem::symlink_status(path, ec));
	}

	bool FilePath::passesThroughSymlink(const FilePath& relative) const {
		auto current = *this;
		for (const auto& part: relative.parts()) {
			current = current / FilePath(part);
			if (!current.isSymlink() && !current.exists()) return false;
			if (current.isSymlink()) return true;
		}
		return false;
	}

	std::vector<std::string> FilePath::parts() const {
		std::vector<std::string> result;
		for (const auto& part: path) result.push_back(part.string());
		return result;
	}

	FilePath FilePath::join(const FilePath& other) const {
		CORE_ASSERT(other.isRelative(), "Cannot join absolute path: " + other.path.string());
		return { path / other.path, type, vfs };
	}

	FilePath FilePath::operator/(const FilePath& other) const { return join(other); }

	FilePath FilePath::toVirtualPath(base::Ref<VFS> target) const {
		if (type == PathType::Virtual) CORE_PANIC("Path is already virtual: " + path.string());
		if (type == PathType::Relative)
			CORE_PANIC(
				"Cannot convert relative path to virtual: " + path.string()
				+ " use absolute() first."
			);
		CORE_ASSERT(type == PathType::Physical, "Unexpected path type: " + path.string());

		auto root     = target->getRootPath();
		auto abs_path = std::filesystem::absolute(path);
		// Manually concatenate strings since operator/ ignores left side for absolute paths
		return { root.generic_string() + abs_path.generic_string(), PathType::Virtual, target };
	}

	FilePath FilePath::toPhysicalPath() const {
		if (type != PathType::Virtual)
			CORE_PANIC("Can only convert virtual paths to physical: " + path.string());

		auto root     = getVfs().value()->getRootPath();
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

	FilePath FilePath::lexicallyNormal() const { return { path.lexically_normal(), type, vfs }; }

	bool FilePath::isAbsolute() const noexcept { return type != PathType::Relative; }

	bool FilePath::exists() const {
		if (type == PathType::Virtual) return vfs.value()->exists(path);
		std::error_code ec;
		return std::filesystem::exists(path, ec);
	}

	bool FilePath::isRegularFile() const {
		if (type == PathType::Virtual) return vfs.value()->isFile(path);
		std::error_code ec;
		return std::filesystem::is_regular_file(path, ec);
	}

	bool FilePath::isDirectory() const {
		if (type == PathType::Virtual) return vfs.value()->isDirectory(path);
		std::error_code ec;
		return std::filesystem::is_directory(path, ec);
	}

	std::expected<timer::Duration, std::string> FilePath::timeSinceModified() const {
		CORE_ASSERT(type != PathType::Virtual, "Virtual files have no modification time");
		std::error_code ec;
		const auto      modified = std::filesystem::last_write_time(path, ec);
		if (ec) return std::unexpected(path.string() + ": " + ec.message());
		return timer::Duration{ std::chrono::duration_cast<timer::Duration::DurationValueT>(
			std::filesystem::file_time_type::clock::now() - modified
		) };
	}

	std::string FilePath::uri() const {
		return filepath_utils::formatFileUri(path.generic_string());
	}

	FilePath FilePath::getDefaultTempDirectoryPath() {
		static FilePath temp_directory = [] {
			FilePath f(std::filesystem::temp_directory_path());
			CORE_ASSERT(f.isTemporary(), "Default temp directory is not temporary");
			return f;
		}();
		return temp_directory;
	}
}

usize std::hash<fs::FilePath>::operator()(const fs::FilePath& key) const {
	usize hash = std::filesystem::hash_value(key.getPath());
	if (const auto* handle = key.vfsHandle())
		hash ^= std::hash<const void*>{}(handle) + 0x9e'37'79'b9 + (hash << 6) + (hash >> 2);
	return hash;
}
