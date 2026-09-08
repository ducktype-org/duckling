#include "file.hpp"

#include <filesystem_private/vfs.hpp>

#include <base/except/exceptions.hpp>
#include <base/misc/shared_view.hpp>
#include <base/pointers/ref.hpp>

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <random>

namespace {
	Ref<fs::VFS> vfs = fs::VFS::getInstance();

	// Validation functions
	void requireDirectory(const fs::File& directory) {
		if (!directory.isDirectory()) CORE_PANIC("Parent is not a directory");
	}

	void requireFile(const fs::File& file) {
		if (!file.isFile()) CORE_PANIC("Path is not a file");
	}

	void requirePhysicalPath(const fs::FilePath& path) {
		if (!path.isPhysical()) CORE_PANIC("Path is not a physical file: " + path.string());
	}

	void requireVirtualPath(const fs::FilePath& path) {
		if (!path.isVirtual()) CORE_PANIC("Path is not a virtual path: " + path.string());
	}

	// Best-effort reason for a stream that would not open. `std::ofstream` reports no
	// error_code, so errno - which the implementation sets on a failed open - is all there is.
	std::string openFailureReason(const fs::FilePath& path) {
		auto reason = errno != 0 ? std::error_code(errno, std::generic_category()).message()
		                         : std::string("could not be opened for writing");
		return path.string() + ": " + reason;
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

			if ((type == fs::FileType::Virtual && vfs->exists(candidate.getPath()))
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
			this->type     = FileType::Virtual;
			this->category = vfs->isDirectory(path) ? FileCategory::Directory : FileCategory::File;
		} else {
			this->path     = path.canonical();
			this->type     = FileType::Physical;
			this->category = std::filesystem::is_directory(path.getPath()) ? FileCategory::Directory
			                                                               : FileCategory::File;
		}
	}

	// --- FileManager static methods ---

	std::expected<File, std::string> FileManager::createPhysicalFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		if (!path.isPhysical())
			return std::unexpected("path is not a physical file path: " + path.string());

		auto            abs_path = path.absolute();
		std::error_code ec;
		if (std::filesystem::exists(abs_path.getPath(), ec) && !allow_overwrite)
			return std::unexpected("physical file already exists: " + abs_path.string());

		errno = 0;
		std::ofstream ofs(abs_path.getPath(), allow_overwrite ? std::ios::trunc : std::ios::out);
		// A stream carries no error_code, so the reason has to come from errno, which the
		// implementation sets on a failed open. It is best effort: a zero errno just means the
		// platform told us nothing more than "it failed".
		if (!ofs) return std::unexpected(openFailureReason(abs_path));
		ofs << content;
		ofs.close();
		if (!ofs) return std::unexpected("failed to write to " + abs_path.string());
		return abs_path;
	}

	File FileManager::createPhysicalFileUnsafe(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requirePhysicalPath(path);

		auto file = createPhysicalFile(path, content, allow_overwrite);
		if (!file) CORE_PANIC("Failed to create physical file: " + file.error());
		return *file;
	}

	std::expected<File, std::string> FileManager::createPhysicalFolder(
		const FilePath& path, bool allow_overwrite
	) {
		if (!path.isPhysical())
			return std::unexpected("path is not a physical folder path: " + path.string());

		auto            abs_path = path.absolute();
		std::error_code ec;
		if (std::filesystem::exists(abs_path.getPath(), ec)) {
			if (!allow_overwrite)
				return std::unexpected("physical folder already exists: " + abs_path.string());
			std::filesystem::remove_all(abs_path.getPath(), ec);
			if (ec) return std::unexpected(ec.message());
		}
		// The bool result only says whether a directory was created, which is false both for a
		// path that already exists and for a failure; the error code is what distinguishes them.
		std::filesystem::create_directories(abs_path.getPath(), ec);
		if (ec) return std::unexpected(ec.message());
		return abs_path;
	}

	File FileManager::createPhysicalFolderUnsafe(const FilePath& path, bool allow_overwrite) {
		requirePhysicalPath(path);

		auto folder = createPhysicalFolder(path, allow_overwrite);
		if (!folder) CORE_PANIC("Failed to create physical folder: " + folder.error());
		return *folder;
	}

	File FileManager::createVirtualFile(
		const FilePath& path, std::string_view content, bool allow_overwrite
	) {
		requireVirtualPath(path);

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

		if (vfs->exists(path.getPath())) {
			if (!allow_overwrite) CORE_PANIC("Virtual folder already exists: " + path.string());
			if (!vfs->isDirectory(path.getPath()))
				CORE_PANIC("Path exists but is not a directory: " + path.string());
			vfs->deleteDirectory(path.getPath(), true);
		}
		vfs->createDirectory(path.getPath());
		return path;
	}

	// The system temp directory is part of the physical filesystem, so a file placed there is
	// created with the physical factories like any other. Only the *random naming* is specific
	// to temporary files, which is all these two helpers add.
	File FileManager::createRandomTempDirectory() {
		FilePath tmp_dir   = FilePath::getDefaultTempDirectoryPath();
		auto     rand_path = randomName(tmp_dir);
		return createPhysicalFolderUnsafe(rand_path);
	}

	File FileManager::createRandomVirtualDirectory() {
		FilePath root      = vfs->getRootPath();
		auto     rand_path = randomName(root);
		vfs->createDirectory(rand_path.getPath());
		return rand_path;
	}

	File FileManager::createRandomVirtualFile(
		std::string_view content, base::Optional<std::string_view> suffix
	) {
		FilePath root      = vfs->getRootPath();
		auto     rand_path = randomName(root);
		if (suffix.has_value())
			rand_path = FilePath(base::strConcat(rand_path.string(), suffix.value()));
		return createVirtualFile(rand_path, content);
	}

	File FileManager::createRandomTempFile(std::string_view content) {
		FilePath tmp_dir   = FilePath::getDefaultTempDirectoryPath();
		auto     rand_path = randomName(tmp_dir);
		return createPhysicalFileUnsafe(rand_path, content);
	}

	bool FileManager::deleteFile(const File& file) {
		if (file.type == FileType::Virtual) return vfs->deleteFile(file.path);
		return std::filesystem::remove(file.path);
	}

	bool FileManager::deleteFolder(const File& folder, bool force) {
		if (folder.type == FileType::Virtual) return vfs->deleteDirectory(folder.path, force);
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
			auto content = vfs->readFile(path);

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

	bool File::isTemporary() const { return path.isTemporary(); }

	bool File::isDirectory() const noexcept { return category == FileCategory::Directory; }

	bool File::isFile() const noexcept { return !isDirectory(); }

	std::string File::extension() const { return path.extension(); }

	std::expected<void, std::string> File::writeToFile(std::string_view new_content, bool append)
		const {
		requireFile(*this);

		if (type == FileType::Virtual) {
			if (!vfs->exists(path))
				return std::unexpected("virtual file does not exist: " + path.string());
			vfs->writeFile(path, new_content, append);
			return {};
		}

		errno                   = 0;
		std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
		std::ofstream      ofs(path.getPath(), mode);
		if (!ofs) return std::unexpected(openFailureReason(path));
		ofs << new_content;
		if (ofs.fail()) return std::unexpected("failed to write to " + path.string());
		return {};
	}

	void File::writeToFileUnsafe(std::string_view new_content, bool append) const {
		auto written = writeToFile(new_content, append);
		if (!written) CORE_PANIC("Failed to write to file: " + written.error());
	}

	File File::createSubFile(std::string_view new_file_content, std::string_view custom_name) const {
		requireDirectory(*this);
		FilePath file_path = genPathInDirectory(*this, custom_name);

		switch (type) {
		case FileType::Virtual:
			FileManager::createVirtualFile(file_path, new_file_content);
			break;
		case FileType::Physical:
			FileManager::createPhysicalFileUnsafe(file_path, new_file_content);
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
		case FileType::Physical:
			FileManager::createPhysicalFolderUnsafe(dir_path);
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
			auto entries = vfs->listDirectory(path);
			for (const auto& entry: entries) result.emplace_back(path / entry);
		} else {
			for (const auto& entry: std::filesystem::directory_iterator(path.getPath()))
				result.emplace_back(entry.path());
		}

		return result;
	}

	File FileManager::getVirtualRootDirectory() { return { vfs->getRootPath() }; }
}
