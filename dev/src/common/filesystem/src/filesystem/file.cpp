#include "file.hpp"

#include <filesystem/vfs.hpp>

#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <algorithm>
#include <fstream>
#include <random>

namespace {
	Ref<fs::VFS> vfs = fs::VFS::getInstance();

	/*
	 * Checks if the given path is in the system's temporary directory.
	 * The path must be real, since we use canonicalization to resolve it.s
	 */
	bool isInTempDirectory(const std::filesystem::path& path) {
		auto temp_dir = std::filesystem::canonical(std::filesystem::temp_directory_path());
		auto abs_path = std::filesystem::canonical(path);

		auto mismatch = std::mismatch(temp_dir.begin(), temp_dir.end(), abs_path.begin());
		return mismatch.first == temp_dir.end();
	}

	/*
	 * Checks if the given path has a prefix that matches the system's temporary directory.
	 * The path does not need to be real (point to an existing file).
	 */
	bool hasTemporaryPrefix(const std::filesystem::path& path) {
		auto temp_dir = std::filesystem::canonical(std::filesystem::temp_directory_path());
		auto abs_path = std::filesystem::absolute(path);

		auto temp_str = temp_dir.generic_string();
		auto path_str = abs_path.generic_string();

		// Ensure temp_str ends with a separator for correct prefix matching
		if (!temp_str.empty() && temp_str.back() != '/') temp_str += '/';

		return path_str.starts_with(temp_str);
	}

	// Validation functions
	void requireDirectory(const fs::File& directory) {
		if (!directory.isDirectory()) CORE_PANIC("Parent is not a directory");
	}

	void requireNotPhysical(const fs::File& file) {
		if (file.getType() == fs::FileType::Physical) CORE_PANIC("File is physical");
	}

	void requireFile(const fs::File& file) {
		if (!file.isFile()) CORE_PANIC("Path is not a file");
	}

	void requirePhysicalPath(const std::filesystem::path& path) {
		if (fs::VFS::isVirtualPath(path) || hasTemporaryPrefix(path))
			CORE_PANIC("Path is not a physical file: " + path.string());
	}

	void requireVirtualPath(const std::filesystem::path& path) {
		if (!fs::VFS::isVirtualPath(path))
			CORE_PANIC("Path is not a virtual path: " + path.string());
	}

	void requireTempPath(const std::filesystem::path& path) {
		if (!hasTemporaryPrefix(path)) CORE_PANIC("Path is not a temporary path: " + path.string());
	}

	std::filesystem::path randomName(
		const std::filesystem::path& prefix_path, const size_t name_len = 16
	) {
		static std::random_device device;
		static std::mt19937       rng(device());
		static std::string        name_chars
			= "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPRQSTUVWXYZ";
		static std::uniform_int_distribution<size_t> dist(0, name_chars.length() - 1);

		// This is 64, but usually it will not loop more than once.
		for (size_t try_no = 0; try_no < 64; try_no++) {
			std::string name;
			for (int i = 0; i < name_len; i++) name.push_back(name_chars[dist(rng)]);
			if (!exists(prefix_path / name)) return prefix_path / name;
		}
		CORE_PANIC("Couldn't create a new name in: " + absolute(prefix_path).string());
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
	std::filesystem::path genPathInDirectory(
		const fs::File& directory, std::string_view custom_name = ""
	) {
		requireDirectory(directory);
		requireNotPhysical(directory);

		auto type = directory.getType();

		std::filesystem::path file_name;
		if (custom_name.empty())
			file_name = randomName(directory.nativePath());
		else {
			file_name = std::filesystem::path(directory.nativePath()) / custom_name;

			if ((type == fs::FileType::Virtual && vfs->exists(file_name))
			    || (type != fs::FileType::Virtual && exists(file_name)))
				CORE_PANIC(base::strConcat(
					"Cannot create a file/dir with name \"",
					custom_name,
					"\", because there already is a file/dir with this name in "
						+ directory.nativePath()
				));
		}
		return file_name;
	}
}

namespace fs {
	File::File(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path)) {
			this->is_symlink = false;  // VFS paths are not symlinks
			this->path       = path;
			this->type       = FileType::Virtual;
			this->category = vfs->isDirectory(path) ? FileCategory::Directory : FileCategory::File;
		} else {
			this->is_symlink = std::filesystem::is_symlink(path);
			this->path       = canonical(path);
			this->type = isInTempDirectory(this->path) ? FileType::Temporary : FileType::Physical;
			this->category
				= is_directory(this->path) ? FileCategory::Directory : FileCategory::File;
		}
	}

	// --- FileManager static methods ---

	File FileManager::getDefaultTempDirectory() {
		static File temp_directory = [] {
			File f(std::filesystem::temp_directory_path());
			CORE_ASSERT(f.type == FileType::Temporary, "Default temp directory is not temporary");
			return f;
		}();
		return temp_directory;
	}

	File FileManager::getDefaultVirtualDirectory() {
		static File virtual_directory_path(vfs->getRootPath());
		return virtual_directory_path;
	}

	File FileManager::createPhysicalFile(
		const std::filesystem::path& path, std::string_view content, bool override_
	) {
		requirePhysicalPath(path);

		std::filesystem::path abs_path = std::filesystem::absolute(path);
		if (std::filesystem::exists(abs_path)) {
			if (!override_) CORE_PANIC("Physical file already exists: " + abs_path.string());
		}
		std::ofstream ofs(abs_path, override_ ? std::ios::trunc : std::ios::out);
		if (!ofs) CORE_PANIC("Failed to create physical file: " + abs_path.string());
		ofs << content;
		ofs.close();
		return abs_path;
	}

	File FileManager::createPhysicalFolder(const std::filesystem::path& path, bool override_) {
		requirePhysicalPath(path);

		std::filesystem::path abs_path = std::filesystem::absolute(path);
		if (std::filesystem::exists(abs_path)) {
			if (!override_) CORE_PANIC("Physical folder already exists: " + abs_path.string());
			std::filesystem::remove_all(abs_path);
		}
		std::filesystem::create_directories(abs_path);
		return abs_path;
	}

	File FileManager::createVirtualFile(
		const std::filesystem::path& path, std::string_view content, bool override_
	) {
		requireVirtualPath(path);

		if (vfs->exists(path)) {
			if (!override_) CORE_PANIC("Virtual file already exists: " + path.string());
			if (!vfs->isFile(path)) CORE_PANIC("Path exists but is not a file: " + path.string());
		} else {
			vfs->createFile(path);
		}
		vfs->writeFile(path, content);
		return path;
	}

	File FileManager::createVirtualFolder(const std::filesystem::path& path, bool override_) {
		requireVirtualPath(path);

		if (vfs->exists(path)) {
			if (!override_) CORE_PANIC("Virtual folder already exists: " + path.string());
			if (!vfs->isDirectory(path))
				CORE_PANIC("Path exists but is not a directory: " + path.string());
			vfs->deleteDirectory(path, true);
		}
		vfs->createDirectory(path);
		return path;
	}

	File FileManager::createTempFile(
		const std::filesystem::path& path, std::string_view content, bool override_
	) {
		requireTempPath(path);

		if (std::filesystem::exists(path)) {
			if (!override_) CORE_PANIC("Temp file already exists: " + path.string());
		}
		std::ofstream ofs(path, override_ ? std::ios::trunc : std::ios::out);
		if (!ofs) CORE_PANIC("Failed to create temp file: " + path.string());
		ofs << content;
		ofs.close();
		return path;
	}

	File FileManager::createTempFolder(const std::filesystem::path& path, bool override_) {
		requireTempPath(path);

		if (std::filesystem::exists(path)) {
			if (!override_) CORE_PANIC("Temp folder already exists: " + path.string());
			std::filesystem::remove_all(path);
		}
		std::filesystem::create_directories(path);
		return path;
	}

	File FileManager::createRandomTempDirectory() {
		auto tmp_dir   = std::filesystem::temp_directory_path();
		auto rand_path = randomName(tmp_dir);
		std::filesystem::create_directory(rand_path);
		return rand_path;
	}

	File FileManager::createRandomVirtualDirectory() {
		auto root      = vfs->getRootPath();
		auto rand_path = randomName(root);
		vfs->createDirectory(rand_path);
		return rand_path;
	}

	File FileManager::createRandomVirtualFile(std::string_view content) {
		auto root      = vfs->getRootPath();
		auto rand_path = randomName(root);
		return createVirtualFile(rand_path, content);
	}

	File FileManager::createRandomTempFile(std::string_view content) {
		auto tmp_dir   = std::filesystem::temp_directory_path();
		auto rand_path = randomName(tmp_dir);
		return createTempFile(rand_path, content);
	}

	bool FileManager::fileExists(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path)) return vfs->exists(path) && vfs->isFile(path);
		return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
	}

	bool FileManager::fileExists(const File& file) {
		if (file.type == FileType::Virtual) return vfs->exists(file.path) && vfs->isFile(file.path);
		return std::filesystem::exists(file.path) && std::filesystem::is_regular_file(file.path);
	}

	bool FileManager::folderExists(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path)) return vfs->exists(path) && vfs->isDirectory(path);
		return std::filesystem::exists(path) && std::filesystem::is_directory(path);
	}

	bool FileManager::folderExists(const File& file) {
		if (file.type == FileType::Virtual)
			return vfs->exists(file.path) && vfs->isDirectory(file.path);
		return std::filesystem::exists(file.path) && std::filesystem::is_directory(file.path);
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

	std::filesystem::path FileManager::toVirtualPath(const std::filesystem::path& path) {
		requirePhysicalPath(path);
		auto root           = vfs->getRootPath();
		auto canonical_path = canonical(path);
		// Manually concatenate strings since operator/ ignores left side for absolute paths
		return root.generic_string() + canonical_path.generic_string();
	}

	std::filesystem::path FileManager::fromVirtualPath(const std::filesystem::path& path) {
		requireVirtualPath(path);

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

		CORE_UNREACHABLE();
	}

	// --- File methods (non-static, formerly FilePath) ---

	FileContent File::getContent() const {
		if (type == FileType::Virtual) {
			auto content = vfs->readFile(path);

			auto r_array = new byte[content.size()];
			std::ranges::copy(content, reinterpret_cast<char*>(r_array));

			return FileContent(std::make_shared<base::OwningView>(r_array, content.size()));
		} else {
			std::ifstream file(path, std::ios::in | std::ios::binary);
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

			return FileContent(std::make_shared<base::OwningView>(r_array, file_size));
		}
		CORE_UNREACHABLE();
	}

	std::expected<FileContent, std::string> File::getContentSafe() const {
		if ((type != FileType::Virtual && !exists(path))
		    || (type == FileType::Virtual && vfs->exists(path))) {
			return std::unexpected(base::strConcat(
				"Error: cannot get content of file `", path, "` - file does not exist"
			));
		}
		return getContent();
	}

	std::string_view File::strView() const { return path.c_str(); }

	File File::parentPath() const { return path.parent_path(); }

	std::string File::nativePath() const { return path.native(); }

	std::string File::uri() const { return "file://" + path.generic_string(); }

	std::string File::name() const {
		if (isDirectory() && path.filename() == ".") return path.parent_path().filename();
		return path.filename();
	}

	bool File::isDirectory() const noexcept {
		if (type == FileType::Virtual) return vfs->isDirectory(path);
		return is_directory(path);
	}

	bool File::isFile() const noexcept { return !isDirectory(); }

	std::string File::stem() const { return path.stem(); }

	std::string File::extension() const { return path.extension(); }

	std::vector<File> File::listFilePaths() const {
		requireDirectory(*this);

		std::vector<File> file_paths;
		if (type == FileType::Virtual)
			for (const auto& name: vfs->listDirectory(path)) file_paths.emplace_back(path / name);
		else
			for (const auto& entry: std::filesystem::directory_iterator(path))
				file_paths.emplace_back(entry.path());
		return file_paths;
	}

	bool File::isSymlink() const noexcept { return is_symlink; }

	void File::writeToFile(std::string_view new_content, bool append) const {
		requireFile(*this);

		if (type == FileType::Virtual) {
			if (!vfs->exists(path)) CORE_PANIC("Virtual file does not exist: " + path.string());
			vfs->writeFile(path, new_content, append);
		} else {
			// Physical or Temporary file
			std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : std::ios::out;
			std::ofstream      ofs(path, mode);
			if (!ofs) CORE_PANIC("Failed to open file for writing: " + path.string());
			ofs << new_content;
			if (ofs.fail()) CORE_PANIC("Failed to write to file: " + path.string());
		}
	}

	u64 File::queryUnstablePerfectHash() const {
		static u64                      next_hash = 0;
		static base::HashMap<File, u64> hash_map;

		// @Future: use atMaybe
		if (hash_map.contains(*this)) return hash_map.at(*this);

		auto hash = next_hash++;

		hash_map.put(*this, hash);
		return hash;
	}

	bool FileManager::isSymlink(const std::filesystem::path& path) noexcept {
		if (VFS::isVirtualPath(path)) return false;
		return std::filesystem::is_symlink(path);
	}

	File FileManager::createFileIn(
		const File& directory, std::string_view new_file_content, std::string_view custom_name
	) {
		requireDirectory(directory);
		std::filesystem::path file_path = genPathInDirectory(directory, custom_name);

		switch (directory.type) {
		case FileType::Virtual:
			if (vfs->exists(file_path))
				CORE_PANIC("File already exists in virtual directory: " + file_path.string());
			vfs->createFile(file_path);
			vfs->writeFile(file_path, new_file_content);
			break;
		case FileType::Temporary:
		case FileType::Physical:
			if (std::filesystem::exists(file_path))
				CORE_PANIC("File already exists in directory: " + file_path.string());
			{
				std::ofstream ofs(file_path);
				if (!ofs) CORE_PANIC("Failed to create file: " + file_path.string());
				ofs << new_file_content;
				ofs.close();
			}
			break;
		default:
			CORE_PANIC("Unsupported directory type for file creation");
		}
		return file_path;
	}

	File FileManager::createDirectoryIn(const File& directory, std::string_view custom_name) {
		requireDirectory(directory);
		std::filesystem::path dir_path = genPathInDirectory(directory, custom_name);

		switch (directory.type) {
		case FileType::Virtual:
			if (vfs->exists(dir_path))
				CORE_PANIC("Directory already exists in virtual directory: " + dir_path.string());
			vfs->createDirectory(dir_path);
			break;
		case FileType::Temporary:
		case FileType::Physical:
			if (std::filesystem::exists(dir_path))
				CORE_PANIC("Directory already exists: " + dir_path.string());
			std::filesystem::create_directory(dir_path);
			break;
		default:
			CORE_PANIC("Unsupported directory type for directory creation");
		}
		return dir_path;
	}
}
