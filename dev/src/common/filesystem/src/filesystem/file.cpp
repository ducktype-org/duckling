/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "file.hpp"

#include "vfs.hpp"

#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/raw_view.hpp>
#include <base/ref.hpp>

#include <algorithm>
#include <fstream>
#include <random>

namespace {
	Ref<fs::VFS> vfs = fs::VFS::getInstance();

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
		throw base::LogicError("Couldn't create a new name in: " + absolute(prefix_path).string());
	}

	bool isInTempDirectory(const std::filesystem::path& path) {
		auto temp_dir = std::filesystem::canonical(std::filesystem::temp_directory_path());
		auto abs_path = std::filesystem::canonical(path);

		auto mismatch = std::mismatch(temp_dir.begin(), temp_dir.end(), abs_path.begin());
		return mismatch.first == temp_dir.end();
	}

	std::filesystem::path genPathInDirectory(
		const fs::File& directory, std::string_view custom_name = ""
	) {
		if (!directory.isDirectory()) throw base::LogicError("Parent is not a directory");
		auto type = directory.getType();
		if (!(type == fs::FileType::Temporary || type == fs::FileType::Virtual))
			throw base::LogicError("Parent is not temporary or virtual");

		std::filesystem::path file_name;
		if (custom_name.empty())
			file_name = randomName(directory.absolutePath());
		else {
			file_name = std::filesystem::path(directory.absolutePath()) / custom_name;

			if ((type == fs::FileType::Virtual && vfs->exists(file_name))
			    || (type != fs::FileType::Virtual && exists(file_name)))
				throw base::LogicError(base::strConcat(
					"Cannot create a file/dir with name \"",
					custom_name,
					"\", because there already is a file/dir with this name in "
						+ directory.absolutePath()
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
		const std::filesystem::path& path, std::string_view content, bool override
	) {
		std::filesystem::path abs_path = std::filesystem::absolute(path);
		if (std::filesystem::exists(abs_path)) {
			if (!override)
				throw base::LogicError("Physical file already exists: " + abs_path.string());
		}
		std::ofstream ofs(abs_path, override ? std::ios::trunc : std::ios::out);
		if (!ofs) throw base::LogicError("Failed to create physical file: " + abs_path.string());
		ofs << content;
		ofs.close();
		return abs_path;
	}

	File FileManager::createPhysicalFolder(const std::filesystem::path& path, bool override) {
		std::filesystem::path abs_path = std::filesystem::absolute(path);
		if (std::filesystem::exists(abs_path)) {
			if (!override)
				throw base::LogicError("Physical folder already exists: " + abs_path.string());
			std::filesystem::remove_all(abs_path);
		}
		std::filesystem::create_directories(abs_path);
		return abs_path;
	}

	File FileManager::createVirtualFile(
		const std::filesystem::path& path, std::string_view content, bool override
	) {
		std::filesystem::path vpath = VFS::isVirtualPath(path) ? path : toVirtualPath(path);
		if (vfs->exists(vpath)) {
			if (!override) throw base::LogicError("Virtual file already exists: " + vpath.string());
			if (!vfs->isFile(vpath))
				throw base::LogicError("Path exists but is not a file: " + vpath.string());
		} else {
			vfs->createFile(vpath);
		}
		vfs->writeFile(vpath, content);
		return vpath;
	}

	File FileManager::createVirtualFolder(const std::filesystem::path& path, bool override) {
		std::filesystem::path vpath = VFS::isVirtualPath(path) ? path : toVirtualPath(path);
		if (vfs->exists(vpath)) {
			if (!override)
				throw base::LogicError("Virtual folder already exists: " + vpath.string());
			if (!vfs->isDirectory(vpath))
				throw base::LogicError("Path exists but is not a directory: " + vpath.string());
			vfs->deleteDirectory(vpath, true);
		}
		vfs->createDirectory(vpath);
		return vpath;
	}

	File FileManager::createTempFile(
		const std::filesystem::path& path, std::string_view content, bool override
	) {
		auto                  tmp_dir = std::filesystem::temp_directory_path();
		std::filesystem::path tpath   = path;
		if (tpath.empty() || tpath.string().find(tmp_dir.string()) != 0) tpath = tmp_dir / path;
		if (std::filesystem::exists(tpath)) {
			if (!override) throw base::LogicError("Temp file already exists: " + tpath.string());
		}
		std::ofstream ofs(tpath, override ? std::ios::trunc : std::ios::out);
		if (!ofs) throw base::LogicError("Failed to create temp file: " + tpath.string());
		ofs << content;
		ofs.close();
		return tpath;
	}

	File FileManager::createTempFolder(const std::filesystem::path& path, bool override) {
		auto                  tmp_dir = std::filesystem::temp_directory_path();
		std::filesystem::path tpath   = path;
		if (tpath.empty() || tpath.string().find(tmp_dir.string()) != 0) tpath = tmp_dir / path;
		if (std::filesystem::exists(tpath)) {
			if (!override) throw base::LogicError("Temp folder already exists: " + tpath.string());
			std::filesystem::remove_all(tpath);
		}
		std::filesystem::create_directories(tpath);
		return tpath;
	}

	File FileManager::createTempDirectory() {
		auto tmp_dir   = std::filesystem::temp_directory_path();
		auto rand_path = randomName(tmp_dir);
		std::filesystem::create_directory(rand_path);
		return rand_path;
	}

	File FileManager::createVirtualDirectory() {
		auto root      = vfs->getRootPath();
		auto rand_path = randomName(root);
		vfs->createDirectory(rand_path);
		return File(rand_path);
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
		auto root = vfs->getRootPath();
		if (path.string().starts_with(root.string())) {
			throw base::LogicError(
				"Cannot convert path to virtual path, because it is already a virtual path"
			);
		}
		return root / canonical(path);
	}

	std::filesystem::path FileManager::fromVirtualPath(const std::filesystem::path& path) {
		if (!VFS::isVirtualPath(path)) {
			throw base::LogicError(
				"Cannot convert path from virtual path, because it is not a virtual path"
			);
		}
		auto root     = vfs->getRootPath();
		auto path_str = path.string();
		auto root_str = root.string();

		// Ensure root_str ends with a separator for correct prefix matching
		if (!root_str.empty() && root_str.back() != std::filesystem::path::preferred_separator)
			root_str += std::filesystem::path::preferred_separator;

		if (path_str.starts_with(root_str)) {
			return std::filesystem::canonical(path_str.substr(root_str.size()));
		} else if (path_str == root_str.substr(0, root_str.size() - 1)) {
			// If path is exactly the root
			return std::filesystem::path{};
		}
		CORE_PANIC(
			"Cannot convert path from virtual path, because it does not start with the VFS root "
			"path"
		);
	}

	// --- File methods (non-static, formerly FilePath) ---

	FileContent File::getContent() const {
		return FileContent(std::make_shared<base::OwningView>(
			type != FileType::Virtual ? getSimpleFileContent(path.c_str())
									  : getSimpleVirtualFileContent(path.c_str())
		));
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

	std::string File::absolutePath() const { return path; }

	std::string File::uri() const { return "file://" + absolutePath(); }

	std::string File::name() const {
		if (isDirectory() && path.filename() == ".") return path.parent_path().filename();
		return path.filename();
	}

	bool File::isDirectory() const noexcept {
		if (type == FileType::Virtual) return vfs->isDirectory(path);
		return is_directory(path);
	}

	std::chrono::file_clock::time_point File::getModifyTime() const {
		if (type == FileType::Virtual)
			throw base::LogicError("Cannot get modify time of virtual file");
		return last_write_time(path);
	}

	bool File::isFile() const noexcept { return !isDirectory(); }

	std::string File::stem() const { return path.stem(); }

	std::string File::extension() const { return path.extension(); }

	std::vector<File> File::listFilePaths() const {
		if (category == FileCategory::File) throw base::LogicError("Path is not a directory");

		std::vector<File> file_paths;
		if (type == FileType::Virtual)
			for (const auto& name: vfs->listDirectory(path)) file_paths.emplace_back(path / name);
		else
			for (const auto& entry: std::filesystem::directory_iterator(path))
				file_paths.emplace_back(entry.path());
		return file_paths;
	}

	bool File::isSymlink() const noexcept { return is_symlink; }

	base::OwningView getSimpleFileContent(const std::filesystem::path& path) {
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

		return { r_array, file_size };
	}

	base::OwningView getSimpleVirtualFileContent(const std::filesystem::path& path) {
		auto content = vfs->readFile(path);
		if (content.empty())
			throw base::LogicError(std::string("virtual file does not exist: ") + path.string());

		auto r_array = new byte[content.size()];
		std::ranges::copy(content, reinterpret_cast<char*>(r_array));

		return { r_array, content.size() };
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
		if (directory.category != FileCategory::Directory)
			throw base::LogicError("Parent is not a directory");
		std::filesystem::path file_path = genPathInDirectory(directory, custom_name);

		if (directory.type == FileType::Virtual) {
			if (vfs->exists(file_path))
				throw base::LogicError(
					"File already exists in virtual directory: " + file_path.string()
				);
			vfs->createFile(file_path);
			vfs->writeFile(file_path, new_file_content);
		} else if (directory.type == FileType::Temporary || directory.type == FileType::Physical) {
			if (std::filesystem::exists(file_path))
				throw base::LogicError("File already exists in directory: " + file_path.string());
			std::ofstream ofs(file_path);
			if (!ofs) throw base::LogicError("Failed to create file: " + file_path.string());
			ofs << new_file_content;
			ofs.close();
		} else {
			throw base::LogicError("Unsupported directory type for file creation");
		}
		return file_path;
	}

	File FileManager::createDirectoryIn(const File& directory, std::string_view custom_name) {
		if (directory.category != FileCategory::Directory)
			throw base::LogicError("Parent is not a directory");
		std::filesystem::path dir_path = genPathInDirectory(directory, custom_name);

		if (directory.type == FileType::Virtual) {
			if (vfs->exists(dir_path))
				throw base::LogicError(
					"Directory already exists in virtual directory: " + dir_path.string()
				);
			vfs->createDirectory(dir_path);
		} else if (directory.type == FileType::Temporary || directory.type == FileType::Physical) {
			if (std::filesystem::exists(dir_path))
				throw base::LogicError("Directory already exists: " + dir_path.string());
			std::filesystem::create_directory(dir_path);
		} else {
			throw base::LogicError("Unsupported directory type for directory creation");
		}
		return dir_path;
	}
}
