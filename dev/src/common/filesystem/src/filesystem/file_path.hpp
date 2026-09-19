#pragma once

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/vfs.hpp>

#include <filesystem>
#include <string>
#include <type_traits>

/**
 * @brief Represents the type of a file path.
 */
MAKE_STRINGIFYABLE_ENUM(fs, u64, PathType,
	Physical,   ///< Physical path on the filesystem.
	Virtual,    ///< Virtual path in the virtual filesystem.
	Temporary,   ///< Temporary path in the system's temporary directory.
	Relative   ///< Relative path.
);

namespace fs {
	/**
	 * @class FilePath
	 * @brief Abstract wrapper over std::filesystem::path with safe path operations.
	 *
	 * Provides a safe interface for path manipulation across different filesystem types
	 * without performing canonicalization operations. Supports conversion between
	 * virtual and physical paths.
	 */
	class FilePath {
	private:
		std::filesystem::path path;
		PathType              type;
		/// The VFS this path resolves against; set if and only if `type == PathType::Virtual`.
		base::Optional<base::Ref<VFS>> vfs;

		/**
		 * @brief Determines the path type based on the path string.
		 * @param path_str The path string to analyze.
		 * @return The determined PathType.
		 */
		static PathType determinePathType(const std::filesystem::path& path);

		/**
		 * @brief Returns the VFS binding a freshly classified path of `type` should carry.
		 */
		static base::Optional<base::Ref<VFS>> defaultVfsFor(PathType type);

		/**
		 * @brief Builds a path with an explicitly chosen type and VFS binding.
		 */
		FilePath(std::filesystem::path path, PathType type, base::Optional<base::Ref<VFS>> vfs):
			  path(std::move(path)),
			  type(type),
			  vfs(vfs) {}

	public:
		FilePath(const FilePath&)            = default;
		FilePath(FilePath&&)                 = default;
		FilePath& operator=(const FilePath&) = default;
		FilePath& operator=(FilePath&&)      = default;
		~FilePath()                          = default;

		/**
		 * @brief Constructor binding a virtual path to an explicit VFS instance.
		 * @note FilePath itself is excluded: it converts to std::filesystem::path, so without
		 * this the template would outrank the copy constructor for a non-const lvalue and
		 * silently change the VFS of the path for example.
		 * @param path_like The path-like object to construct from; must be a virtual path.
		 * @param vfs The vfs of the path. If empty and the path is virtual,
		 * then chooses the default VFS instance.
		 */
		template<typename T>
		requires std::is_convertible_v<T, std::filesystem::path>
		          && (!std::is_same_v<std::remove_cvref_t<T>, FilePath>)
		FilePath(T&& path_like, base::Optional<base::Ref<VFS>> vfs = {}):
			  path(std::forward<T>(path_like)),
			  type(determinePathType(path)),
			  vfs(type == PathType::Virtual ? vfs.copyValueOr(VFS::getInstance())
		                                    : base::Optional<base::Ref<VFS>>{}) {}

		/**
		 * @brief Gets the VFS this path resolves against, empty for non-virtual paths.
		 */
		[[nodiscard]] const base::Optional<base::Ref<VFS>>& getVfs() const noexcept { return vfs; }

		/**
		 * @brief Gets the underlying std::filesystem::path.
		 * @return The filesystem path.
		 */
		[[nodiscard]] const std::filesystem::path& getPath() const noexcept { return path; }

		/**
		 * @brief Gets the path type.
		 * @return The PathType of this path.
		 */
		[[nodiscard]] PathType getType() const noexcept { return type; }

		/**
		 * @brief Gets the string representation of the path.
		 * @return String representation of the path.
		 */
		[[nodiscard]] std::string string() const { return path.string(); }

		/**
		 * @brief Gets the string view representation of the path.
		 * @return String view representation of the path.
		 */
		[[nodiscard]] std::string_view strView() const { return path.c_str(); }

		/**
		 * @brief Gets the generic string representation of the path.
		 * @return Generic string representation of the path.
		 */
		[[nodiscard]] std::string genericString() const { return path.generic_string(); }

		/**
		 * @brief Gets the native string representation of the path.
		 * @return Native string representation of the path.
		 */
		[[nodiscard]] std::string native() const { return path.native(); }

		/**
		 * @brief Gets the name component of the path (filename for files, directory name for
		 * directories).
		 * @return String representing the name.
		 */
		[[nodiscard]] std::string name() const {
			return std::filesystem::absolute(path).filename();
		}

		/**
		 * @brief Gets the parent path.
		 * @return FilePath representing the parent directory.
		 */
		[[nodiscard]] FilePath parentPath() const { return { path.parent_path(), type, vfs }; }

		/**
		 * @brief Gets the file extension.
		 * @return String representing the file extension.
		 */
		[[nodiscard]] std::string extension() const { return path.extension().string(); }

		/**
		 * @brief Gets the stem (filename without extension).
		 * @return String representing the stem.
		 */
		[[nodiscard]] std::string stem() const { return path.stem().string(); }

		/**
		 * @brief Gets the canonical path.
		 * @return FilePath representing the canonical path.
		 * @throws std::filesystem::filesystem_error if the path does not exist or cannot be resolved.
		 */
		[[nodiscard]] FilePath canonical() const;

		/**
		 * @brief Checks if the path is a symbolic link.
		 * @return True if the path is a symbolic link, false otherwise.
		 */
		[[nodiscard]] bool isSymlink() const;

		/**
		 * @brief Safely joins this path with another FilePath.
		 * @param other The FilePath to join.
		 * @return New FilePath with the joined path.
		 */
		[[nodiscard]] FilePath join(const FilePath& other) const;

		/**
		 * @brief Operator/ for FilePath joining.
		 * @param other The FilePath to join.
		 * @return New FilePath with the joined path.
		 */
		[[nodiscard]] FilePath operator/(const FilePath& other) const;

		/**
		 * @brief Converts this path to a virtual path.
		 * @note Only physical paths can be converted to virtual paths.
		 * @return FilePath representing the virtual path.
		 * @throws CORE_PANIC if the path is already virtual or conversion fails.
		 */
		[[nodiscard]] FilePath toVirtualPath(base::Optional<base::Ref<VFS>> target = {}) const;

		/**
		 * @brief Converts this virtual path to a physical path.
		 * This replaces the vfs root directory of the path
		 * with the physical filesystem root directory.
		 * @return FilePath representing the physical path.
		 * @throws CORE_PANIC if the path is not virtual or conversion fails.
		 */
		[[nodiscard]] FilePath toPhysicalPath() const;

		/**
		 * @brief Checks if this path is virtual.
		 * @return True if the path is virtual, false otherwise.
		 */
		[[nodiscard]] bool isVirtual() const noexcept { return type == PathType::Virtual; }

		/**
		 * @brief Checks if this path is physical.
		 * @return True if the path is physical, false otherwise.
		 */
		[[nodiscard]] bool isPhysical() const noexcept { return type == PathType::Physical; }

		/**
		 * @brief Checks if this path is temporary.
		 * @return True if the path is temporary, false otherwise.
		 */
		[[nodiscard]] bool isTemporary() const noexcept { return type == PathType::Temporary; }

		/**
		 * @brief Makes this path absolute without canonicalization.
		 * @note the RelativePath type will be changed to Physical.
		 * This will affect only Relative paths; other types remain unchanged.
		 * @return FilePath representing the absolute path.
		 */
		[[nodiscard]] FilePath absolute() const;

		/**
		 * @brief Removes redundant `.` and `..` components without touching the filesystem.
		 * @return FilePath representing the normalized path.
		 */
		[[nodiscard]] FilePath lexicallyNormal() const;

		/**
		 * @brief Returns the same virtual path bound to a different VFS instance.
		 */
		[[nodiscard]] FilePath withVfs(base::Ref<VFS> other) const {
			CORE_ASSERT(
				type == PathType::Virtual, "Only virtual paths can be rebound: " + path.string()
			);
			return { path, type, other };
		}

		/**
		 * @brief Checks if the path is absolute.
		 * @note Virtual/Relative/Temporary paths are always considered absolute.
		 * @return True if the path is absolute, false otherwise.
		 */
		[[nodiscard]] bool isAbsolute() const noexcept;

		/**
		 * @brief Checks if the path is relative.
		 * @note Virtual/Relative/Temporary paths are never considered relative.
		 * @return True if the path is relative, false otherwise.
		 */
		[[nodiscard]] bool isRelative() const noexcept { return !isAbsolute(); }

		/**
		 * @brief Checks if the path is empty.
		 * @return True if the path is empty, false otherwise.
		 */
		[[nodiscard]] bool empty() const noexcept { return path.empty(); }

		/**
		 * @brief Checks if the path exists.
		 * @return True if the path exists, false otherwise.
		 */
		[[nodiscard]] bool exists() const;

		/**
		 * @brief Checks if the path points to an existing regular file.
		 * @return True if the path is a regular file, false otherwise (a directory, a special
		 * file, or a path that does not exist).
		 */
		[[nodiscard]] bool isRegularFile() const;

		[[nodiscard]] std::string uri() const;

		/**
		 * Returns the default temporary directory as a FilePath.
		 */
		[[nodiscard]] static FilePath getDefaultTempDirectoryPath();

		/**
		 * Returns the default virtual directory as a FilePath.
		 */
		[[nodiscard]] static FilePath getDefaultVirtualDirectoryPath();

		// Comparison operators
		auto operator<=>(const FilePath& other) const {
			CORE_ASSERT(
				(path != other.path) || (type == other.type),
				"FilePath differ in exactly one of type/path, this should never happen"
			);
			if (auto cmp = path <=> other.path; cmp != std::strong_ordering::equal) return cmp;
			return vfsHandle() <=> other.vfsHandle();
		}

		auto operator==(const FilePath& other) const {
			CORE_ASSERT(
				(path != other.path) || (type == other.type),
				base::strConcat(
					"FilePath differ in exactly one of type/path, this should never happen: ",
					path.string(),
					" vs ",
					other.path.string(),
					" (type: ",
					type,
					" vs ",
					other.type,
					")"
				)
			);
			return path == other.path && vfsHandle() == other.vfsHandle();
		}

		/**
		 * @brief Returns the bound VFS as a raw pointer, null when unbound.
		 */
		[[nodiscard]] const VFS* vfsHandle() const noexcept {
			return vfs.has_value() ? vfs.value().get() : nullptr;
		}

		// Conversion to std::filesystem::path
		operator std::filesystem::path() const { return path; }
	};
}

/**
 * @brief Hashes a path together with the VFS instance it is bound to.
 */
template<>
struct std::hash<fs::FilePath> final {
	usize operator()(const fs::FilePath& key) const;
};
