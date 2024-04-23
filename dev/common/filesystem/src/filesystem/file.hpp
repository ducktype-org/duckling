/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#pragma once

#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <memory>
#include <base/raw_view.hpp>
#include <base/smart_pointers.hpp>
#include <base/maps.hpp>
#include <base/optional.hpp>
#include <base/perfect_hash.hpp>
#include <result.hpp>

// Seams fixed:
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
		FileContent(): content(nullptr){};
		FileContent(const FileContent&) = default;
		FileContent(FileContent&&)      = default;

		FileContent& operator=(const FileContent&) = default;
		FileContent& operator=(FileContent&&)      = default;

		usize size() { return view().size(); }

		byte operator[](usize i) { return view()[i]; }

		base::RawView view() { return content->view(); }
	};

	class FilePath {
		using WeakContent = std::weak_ptr<base::OwningView>;
		using FileHash    = std::hash<std::filesystem::path>;
		using ContentMap  = base::HashMap<std::filesystem::path, WeakContent, FileHash>;

		// This might be hidden in .cpp:
		static ContentMap     to_content;
		std::filesystem::path path;

		friend struct ::std::hash<fs::FilePath>;

	public:
		FilePath(const FilePath&) = default;
		FilePath(FilePath&&)      = default;
		~FilePath()               = default;

		FilePath(const std::filesystem::path& path):
			  path(std::filesystem::canonical(std::filesystem::absolute(path))) {}

		// @TODO: this might not be perfect:
		bool operator==(const FilePath& oth) const { return path == oth.path; }

		static FilePath createTempFile(const std::string& content) {
			const std::string name = std::tmpnam(nullptr);

			std::fstream temp_file(name, std::ios::out | std::ios::app);
			temp_file << content;
			temp_file.close();

			return { std::filesystem::temp_directory_path() / name };
		}

		[[nodiscard]]
		FileContent getContent() const;
		[[nodiscard]]
		cpp::result<FileContent, std::string> getContentSafe() const;

		[[nodiscard]]
		std::string_view strView() const;

		[[nodiscard]]
		FilePath parentPath() const;

		[[nodiscard]]
		std::string absolutePath() const;

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
struct std::hash<fs::FilePath> {
	usize operator()(const fs::FilePath& key) const { return fs::FilePath::FileHash()(key.path); }
};
