/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/perfect_hash.hpp>
#include <base/raw_view.hpp>

#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

// Seems fixed:
// #if __GNUC__ < 12 && (!defined(__clang__))
// // @GCC12: This specialization is in C++17, but is suported by GCC only from version 12
// // Delete this code when switched to GCC12 (currently GCC10/GCC11 is used)
// template<>
// struct std::hash<std::filesystem::path> {
// 	size_t operator()(const std::filesystem::path& path) const {
// 		return std::filesystem::hash_value(path);
// 	}
// };
// #endif

namespace fs {

	class FileContent {
		std::shared_ptr<base::OwningView> content;
		friend class FilePath;

		explicit FileContent(std::shared_ptr<base::OwningView> content):
			  content(std::move(content)) {}

	public:
		FileContent(): content(nullptr) {}

		FileContent(const FileContent&) = default;
		FileContent(FileContent&&)      = default;

		FileContent& operator=(const FileContent&) = default;
		FileContent& operator=(FileContent&&)      = default;

		usize size() { return view().size(); }

		byte operator[](usize i) { return view()[i]; }

		base::RawView view() { return content->view(); }

		[[nodiscard]]
		const base::RawView view() const {
			return content->view();
		}
	};

	class FilePath {
		using WeakContent = std::weak_ptr<base::OwningView>;
		using FileHash    = std::hash<std::filesystem::path>;
		using ContentMap  = base::HashMap<std::filesystem::path, WeakContent, FileHash>;

		// This might be hidden in .cpp:
		static ContentMap     to_content;
		std::filesystem::path path;

		friend struct std::hash<FilePath>;

		static FilePath getDefaultTempPath();


		/**
		 * A flag indicating the object is located in a system's temporary path.
		 */
		bool is_temporary = false;

		/**
		 * Creates a FilePath and sets is_temporary to true.
		 * @param path Path of new FilePath object.
		 * @return A new FilePath object.
		 */
		static FilePath createTempFilePathObj(const std::filesystem::path& path);

		/**
		 * A helper function creating a new unique path with a prefix of this object's path.
		 * A custom name for new filesystem file/directory. If custom_name is default, then creates
		 * a random name. It is required that this object is a temporary directory.
		 * @param custom_name A custom name for new filesystem file/directory. If left default
		 * creates a random name.
		 * @return A new, guaranteed to be unique path.
		 */
		[[nodiscard]]
		std::filesystem::path genTempPathInMe(std::string_view custom_name = "") const;

	public:
		FilePath& operator=(const FilePath&) = default;
		FilePath& operator=(FilePath&&)      = default;
		FilePath(const FilePath&)            = default;
		FilePath(FilePath&&)                 = default;
		~FilePath()                          = default;

		FilePath(const std::filesystem::path& path): path(canonical(absolute(path))) {}

		// @TODO: this might not be perfect:
		bool operator==(const FilePath& oth) const { return path == oth.path; }

		/**
		 * Creates a new directory inside this object's path. It is required that this object is a
		 * temporary directory.
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A path to the newly created directory.
		 */
		[[nodiscard]]
		FilePath createTempDirectoryIn(std::string_view custom_name = "") const;

		/**
		 * Creates a temporary directory. The directory is managed by the system and has a random
		 * name.
		 * @return A FilePath with the new temporary directory.
		 */
		static FilePath createTempDirectory();


		/**
		 * Creates a new file inside this object's path. It is required that this object is a
		 * temporary directory.
		 * @param new_file_content Content of the file to be created
		 * @param custom_name A custom name. If left default then creates a new random name.
		 * @return A path to the newly created file.
		 */
		[[nodiscard]]
		FilePath createTempFileIn(
			std::string_view new_file_content, std::string_view custom_name = ""
		) const;
		/**
		 * Creates a temporary file with a given content. The file is managed by the system and has
		 * a random name.
		 * @param content The content, that will be inserted into the a file.
		 * @return A FilePath with the new temporary file.
		 */
		static FilePath createTempFile(std::string_view content);


		[[nodiscard]]
		FileContent getContent() const;
		[[nodiscard]]
		std::expected<FileContent, std::string> getContentSafe() const;

		[[nodiscard]]
		std::string_view strView() const;

		[[nodiscard]]
		FilePath parentPath() const;

		[[nodiscard]]
		std::string absolutePath() const;

		[[nodiscard]]
		std::string uri() const;

		[[nodiscard]]
		std::string name() const;

		[[nodiscard]]
		std::chrono::file_clock::time_point getModifyTime() const;

		[[nodiscard]]
		bool isFile() const noexcept;

		[[nodiscard]]
		bool isDirectory() const noexcept;

		[[nodiscard]]
		std::filesystem::directory_iterator directoryIterator() const;

		[[nodiscard]]
		std::string stem() const;
		[[nodiscard]]
		std::string extension() const;

		bool operator<(const FilePath& oth) const { return path < oth.path; }

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	base::OwningView getSimpleFileContent(const std::string& file_name);
}

template<>
struct std::hash<fs::FilePath> final {
	usize operator()(const fs::FilePath& key) const { return fs::FilePath::FileHash()(key.path); }
};
