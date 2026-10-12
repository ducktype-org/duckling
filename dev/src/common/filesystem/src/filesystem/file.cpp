// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file.hpp"

#include <base/except/exceptions.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/ref.hpp>

#include <filesystem/vfs.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

namespace {
	/**
	 * @brief Returns the VFS a virtual path resolves against.
	 */
	Ref<fs::VFS> vfsOf(const fs::FilePath& path) { return path.getVfs().value(); }

	// Validation functions
	void requireDirectory(const fs::File& directory) {
		if (!directory.isDirectory()) CORE_PANIC("Parent is not a directory");
	}

	void requireFile(const fs::File& file) {
		if (!file.isFile()) CORE_PANIC("Path is not a file");
	}

	void requirePhysicalPath(const fs::FilePath& path) {
		// @TODO: #3398 make this work in tmp/
		if (!path.isPhysical()) CORE_PANIC("Path is not a physical file: " + path.string());
	}

	void requireVirtualPath(const fs::FilePath& path) {
		if (!path.isVirtual()) CORE_PANIC("Path is not a virtual path: " + path.string());
	}

	void requireTempPath(const fs::FilePath& path) {
		if (!path.isTemporary()) CORE_PANIC("Path is not a temporary path: " + path.string());
	}

	fs::FilePath randomName(const fs::FilePath& prefix_path, const size_t name_len = 16) {
		static std::random_device device;
		static std::mt19937       rng(device());
		static std::string        name_chars
			= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRQSTUVWXYZ";
		static std::uniform_int_distribution<size_t> dist(0, name_chars.length() - 1);

		// This is 64, but usually it will not loop more than once.
		for (size_t try_no = 0; try_no < 64; try_no++) {
			std::string name;
			for (int i = 0; i < name_len; i++) name.push_back(name_chars[dist(rng)]);
			auto candidate = prefix_path / name;
			if (!exists(candidate.getPath())) return candidate;
		}
		CORE_PANIC("Couldn't create a new name in: " + prefix_path.absolute().string());
	}

	/**
	 * @brief Generates a file or directory path inside the given directory.
	 *
	 * If a custom name is provided, uses it; otherwise, generates a random unique name.
	 * Ensures the resulting path does not already exist in the directory.
	 *
	 * @param directory The parent directory as a File.
	 * @param custom_name Optional custom name for the file or directory
	 * @return The generated path inside the directory.
	 */
	fs::FilePath genPathInDirectory(const fs::File& directory, std::string_view custom_name = "") {
		requireDirectory(directory);

		auto         type     = directory.getType();
		fs::FilePath dir_path = directory.getFilePath();

		if (custom_name.empty()) {
			return randomName(dir_path);
		} else {
			auto candidate = dir_path / custom_name;

			if ((type == fs::FileType::Virtual && vfsOf(candidate)->exists(candidate.getPath()))
			    || (type != fs::FileType::Virtual && exists(candidate.getPath())))
				CORE_PANIC(base::strConcat(
					"Cannot create a file/dir with name \"",
					custom_name,
					"\", because there already is a file/dir with this name in "
						+ directory.getFilePath().native()
				));
			return candidate;
		}
	}
}

namespace fs {
	File::File(const FilePath& path): path(path) {
		if (!path.exists())
			CORE_PANIC(
				base::strConcat(
					"Cannot create file because the path does not point to an existing file: "
				),
				path.string()
			);

		if (path.isVirtual()) {
			this->type = FileType::Virtual;
			this->category
				= vfsOf(path)->isDirectory(path) ? FileCategory::Directory : FileCategory::File;
		} else {
			this->path     = path.canonical();
			this->type     = path.isTemporary() ? FileType::Temporary : FileType::Physical;
			this->category = std::filesystem::is_directory(path.getPath()) ? FileCategory::Directory
			                                                               : FileCategory::File;
		}
	}

	// --- FileManager static methods ---

	File FileManager::createPhysicalFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requirePhysicalPath(path);

		auto abs_path = path.absolute();
		if (std::filesystem::exists(abs_path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Physical file already exists: " + abs_path.string());
		}
		std::ofstream ofs(abs_path.getPath(), allow_overwrite ? std::ios::trunc : std::ios::out);
		if (!ofs) CORE_PANIC("Failed to create physical file: " + abs_path.string());
		ofs << content;
		ofs.close();
		return abs_path;
	}

	File FileManager::createPhysicalFolder(const FilePath& path, bool allow_overwrite) {
		requirePhysicalPath(path);

		auto abs_path = path.absolute();
		if (std::filesystem::exists(abs_path.getPath())) {
			if (!allow_overwrite)
				CORE_PANIC("Physical folder already exists: " + abs_path.string());
			std::filesystem::remove_all(abs_path.getPath());
		}
		std::filesystem::create_directories(abs_path.getPath());
		return abs_path;
	}

	File FileManager::createVirtualFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requireVirtualPath(path);

		auto vfs = vfsOf(path);

		if (vfs->exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Virtual file already exists: " + path.string());
			if (!vfs->isFile(path.getPath()))
				CORE_PANIC("Path exists but is not a file: " + path.string());
		} else {
			vfs->createFile(path.getPath());
		}
		vfs->writeFile(path.getPath(), content);
		return path;
	}

	File FileManager::createVirtualFolder(const FilePath& path, bool allow_overwrite) {
		requireVirtualPath(path);

		auto vfs = vfsOf(path);

		if (vfs->exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Virtual folder already exists: " + path.string());
			if (!vfs->isDirectory(path.getPath()))
				CORE_PANIC("Path exists but is not a directory: " + path.string());
			vfs->deleteDirectory(path.getPath(), true);
		}
		vfs->createDirectory(path.getPath());
		return path;
	}

	File FileManager::createTempFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requireTempPath(path);

		if (std::filesystem::exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Temp file already exists: " + path.string());
		}
		std::ofstream ofs(path.getPath(), allow_overwrite ? std::ios::trunc : std::ios::out);
		if (!ofs) CORE_PANIC("Failed to create temp file: " + path.string());
		ofs << content;
		ofs.close();
		return path;
	}

	File FileManager::createTempFolder(const FilePath& path, bool allow_overwrite) {
		requireTempPath(path);

		if (std::filesystem::exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Temp folder already exists: " + path.string());
			std::filesystem::remove_all(path.getPath());
		}
		std::filesystem::create_directories(path.getPath());
		return path;
	}

	File FileManager::createRandomTempDirectory() {
		FilePath tmp_dir   = std::filesystem::temp_directory_path();
		auto     rand_path = randomName(tmp_dir);
		std::filesystem::create_directory(rand_path.getPath());
		return rand_path;
	}

	File FileManager::createRandomVirtualDirectory(base::Ref<VFS> vfs) {
		FilePath root(vfs->getRootPath(), vfs);
		auto     rand_path = randomName(root);
		vfs->createDirectory(rand_path.getPath());
		return rand_path;
	}

	File FileManager::createRandomVirtualFile(
		std::string_view content, base::Optional<std::string_view> suffix, base::Ref<VFS> vfs
	) {
		FilePath root(vfs->getRootPath(), vfs);
		auto     rand_path = randomName(root);
		if (suffix.has_value())
			rand_path = FilePath(base::strConcat(rand_path.string(), suffix.value()), vfs);
		return createVirtualFile(rand_path, content);
	}

	File FileManager::createRandomTempFile(std::string_view content) {
		FilePath tmp_dir   = std::filesystem::temp_directory_path();
		auto     rand_path = randomName(tmp_dir);
		return createTempFile(rand_path, content);
	}

	bool FileManager::deleteFile(const File& file) {
		if (file.type == FileType::Virtual) return vfsOf(file.path)->deleteFile(file.path);
		return std::filesystem::remove(file.path);
	}

	bool FileManager::deleteFolder(const File& folder, bool force) {
		if (folder.type == FileType::Virtual)
			return vfsOf(folder.path)->deleteDirectory(folder.path, force);
		if (force) {
			std::error_code ec;
			std::filesystem::remove_all(folder.path, ec);
			return !ec;
		} else {
			return std::filesystem::remove(folder.path);
		}
	}

	// --- File methods (non-static, formerly FilePath) ---

	base::SharedView File::getContent() const {
		if (type == FileType::Virtual) {
			auto content = vfsOf(path)->readFile(path);

			auto r_array = new byte[content.size()];
			std::ranges::copy(content, reinterpret_cast<char*>(r_array));

			return { r_array, content.size() };
		} else {
			std::ifstream file(path.getPath(), std::ios::in | std::ios::binary);
			if (file.fail()) CORE_PANIC("Failed to open file: " + path.string() + ", error: ");

			file.unsetf(std::ios::skipws);

			auto fpos = file.tellg();
			file.seekg(0, std::ios::end);
			std::streamoff fsize     = file.tellg() - fpos;
			auto           file_size = static_cast<usize>(fsize);
			file.seekg(0, std::ios::beg);

			// should read full file:
			auto r_array = new byte[file_size];
			file.read(reinterpret_cast<char*>(r_array), std::streamsize(file_size));

			return { r_array, file_size };
		}
		CORE_UNREACHABLE();
	}

	std::expected<base::SharedView, std::string> File::getContentSafe() const {
		if (!exists()) {
			return std::unexpected(base::strConcat(
				"Error: cannot get content of file `", path.native(), "` - file does not exist"
			));
		}
		return getContent();
	}

	bool File::exists() const { return path.exists(); }

	std::string File::name() const { return path.name(); }

	std::string File::stem() const { return path.stem(); }

	bool File::isDirectory() const noexcept { return category == FileCategory::Directory; }

	bool File::isFile() const noexcept { return !isDirectory(); }

	std::string File::extension() const { return path.extension(); }

	void File::writeToFile(std::string_view new_content, bool append) const {
		requireFile(*this);

		if (type == FileType::Virtual) {
			if (!vfsOf(path)->exists(path))
				CORE_PANIC("Virtual file does not exist: " + path.string());
			vfsOf(path)->writeFile(path, new_content, append);
		} else {
			// Physical or Temporary file
			std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
			std::ofstream      ofs(path.getPath(), mode);
			if (!ofs) CORE_PANIC("Failed to open file for writing: " + path.string());
			ofs << new_content;
			if (ofs.fail()) CORE_PANIC("Failed to write to file: " + path.string());
		}
	}

	File File::createSubFile(std::string_view new_file_content, std::string_view custom_name) const {
		requireDirectory(*this);
		FilePath file_path = genPathInDirectory(*this, custom_name);

		switch (type) {
		case FileType::Virtual:
			FileManager::createVirtualFile(file_path, new_file_content);
			break;
		case FileType::Temporary:
			FileManager::createTempFile(file_path, new_file_content);
			break;
		case FileType::Physical:
			FileManager::createPhysicalFile(file_path, new_file_content);
			break;
		default:
			CORE_PANIC("Unsupported directory type for file creation");
		}
		return file_path;
	}

	File File::createSubDirectory(std::string_view custom_name) const {
		requireDirectory(*this);
		FilePath dir_path = genPathInDirectory(*this, custom_name);

		switch (type) {
		case FileType::Virtual:
			FileManager::createVirtualFolder(dir_path);
			break;
		case FileType::Temporary:
			FileManager::createTempFolder(dir_path);
			break;
		case FileType::Physical:
			FileManager::createPhysicalFolder(dir_path);
			break;
		default:
			CORE_PANIC("Unsupported directory type for directory creation");
		}
		return dir_path;
	}

	std::vector<FilePath> File::listFilePaths() const {
		if (!isDirectory())
			CORE_PANIC("Cannot list contents: not a directory (" + path.native() + ")");

		std::vector<FilePath> result;

		if (type == FileType::Virtual) {
			// Virtual filesystem
			auto entries = vfsOf(path)->listDirectory(path);
			for (const auto& entry: entries) result.emplace_back(path / entry);
		} else {
			// Physical or Temporary filesystem
			for (const auto& entry: std::filesystem::directory_iterator(path.getPath()))
				result.emplace_back(entry.path());
		}

		return result;
	}
}
