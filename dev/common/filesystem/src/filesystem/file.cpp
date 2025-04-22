/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "file.hpp"

#include "vfs.hpp"

#include "base/raw_view.hpp"
#include <base/exceptions.hpp>
#include <base/maps.hpp>
#include <base/perfect_hash.hpp>

#include <algorithm>
#include <fstream>
#include <random>

namespace {
	std::filesystem::path random_name(
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

	VFS vfs;
}

namespace fs {
	FilePath::FilePath(const std::filesystem::path& path) {
		if (VFS::isVirtualPath(path)) {
			this->path = path;
			this->type = FileType::Virtual;
		} else {
			this->path = canonical(absolute(path));
		}
	}

	FilePath::ContentMap FilePath::to_content;

	FilePath FilePath::getDefaultTempPath() {
		static FilePath temp_directory_path = createFilePathObj(
			std::filesystem::temp_directory_path(), FileType::Temporary, FileCategory::Directory
		);
		return temp_directory_path;
	}

	FilePath FilePath::getDefaultVirtualPath() {
		static FilePath virtual_directory_path
			= createFilePathObj(vfs.getRootPath(), FileType::Virtual, FileCategory::Directory);
		return virtual_directory_path;
	}

	std::filesystem::path FilePath::genPathInMe(std::string_view custom_name) const {
		if (!(category == FileCategory::Directory))
			throw base::LogicError("Parent is not a directory");
		if (!(type == FileType::Temporary || type == FileType::Virtual))
			throw base::LogicError("Parent is not temporary or virtual");

		std::filesystem::path file_name;
		if (custom_name.empty())
			file_name = random_name(path);
		else {
			file_name = path / custom_name;

			if ((type == FileType::Virtual && vfs.exists(file_name))
			    || (type != FileType::Virtual && exists(file_name)))
				throw base::LogicError(base::strConcat(
					"Cannot create a file/dir with name \"",
					custom_name,
					"\", because there already is a file/dir with this name in " + absolutePath()
				));
		}
		return file_name;
	}

	FilePath FilePath::createFilePathObj(
		const std::filesystem::path& path, FileType type, FileCategory category
	) {
		auto&& obj   = FilePath(path);
		obj.type     = type;
		obj.category = category;
		return obj;
	}

	FilePath FilePath::createTempFile(std::string_view content) {
		return getDefaultTempPath().createFileIn(content);
	}

	FilePath FilePath::createVirtualFile(std::string_view content) {
		return getDefaultVirtualPath().createFileIn(content);
	}

	FilePath FilePath::createTempDirectory() { return getDefaultTempPath().createDirectoryIn(); }

	FilePath FilePath::createVirtualDirectory() {
		return getDefaultVirtualPath().createDirectoryIn();
	}

	FilePath FilePath::createDirectoryIn(std::string_view custom_name) const {
		if (!(type == FileType::Temporary || type == FileType::Virtual))
			throw base::LogicError("Parent is not temporary or virtual");
		if (!(category == FileCategory::Directory))
			throw base::LogicError("Parent is not a directory");

		auto&& new_temp_dir = genPathInMe(custom_name);
		if (type != FileType::Virtual)
			std::filesystem::create_directory(new_temp_dir);
		else
			vfs.createDirectory(new_temp_dir);
		return createFilePathObj(new_temp_dir, type, FileCategory::Directory);
	}

	FilePath FilePath::createFileIn(std::string_view new_file_content, std::string_view custom_name)
		const {
		if (!(type == FileType::Temporary || type == FileType::Virtual))
			throw base::LogicError("Parent is not temporary or virtual");
		if (!(category == FileCategory::Directory))
			throw base::LogicError("Parent is not a directory");

		auto&& new_temp_file = genPathInMe(custom_name);

		if (type != FileType::Virtual) {
			std::ofstream temp_file(new_temp_file);
			temp_file << new_file_content;
			temp_file.close();
		} else {
			vfs.createFile(new_temp_file);
			vfs.writeFile(new_temp_file, std::string(new_file_content));
		}

		return createFilePathObj(new_temp_file, type, FileCategory::File);
	}

	FileContent FilePath::getContent() const {
		if (to_content.contains(path)) {
			auto weak_content = to_content[path];
			if (weak_content.expired())
				to_content.erase(path);
			else
				return FileContent(weak_content.lock());
		}

		FileContent file_content(std::make_shared<base::OwningView>(
			type != FileType::Virtual ? getSimpleFileContent(path.c_str())
									  : getSimpleVirtualFileContent(path.c_str())
		));

		to_content.put(path, file_content.content);

		return file_content;
	}

	std::expected<FileContent, std::string> FilePath::getContentSafe() const {
		if ((type != FileType::Virtual && !exists(path))
		    || (type == FileType::Virtual && vfs.exists(path))) {
			return std::unexpected(base::strConcat(
				"Error: cannot get content of file `", path, "` - file does not exist"
			));
		}
		return getContent();
	}

	std::string_view FilePath::strView() const { return path.c_str(); }

	FilePath FilePath::parentPath() const { return path.parent_path(); }

	std::string FilePath::absolutePath() const { return path; }

	std::string FilePath::uri() const { return "file://" + absolutePath(); }

	std::string FilePath::name() const {
		if (isDirectory() && path.filename() == ".") return path.parent_path().filename();
		return path.filename();
	}

	bool FilePath::isDirectory() const noexcept {
		if (type == FileType::Virtual) return vfs.isDirectory(path);
		return is_directory(path);
	}

	std::chrono::file_clock::time_point FilePath::getModifyTime() const {
		if (type == FileType::Virtual)
			throw base::LogicError("Cannot get modify time of virtual file");
		return last_write_time(path);
	}

	bool FilePath::isFile() const noexcept { return !isDirectory(); }

	std::string FilePath::stem() const { return path.stem(); }

	std::string FilePath::extension() const { return path.extension(); }

	std::filesystem::directory_iterator FilePath::directoryIterator() const {
		if (type == FileType::Virtual)
			throw base::LogicError("Cannot get directory iterator of virtual file");
		return std::filesystem::directory_iterator(path);
	}

	std::vector<std::string> FilePath::listDirectory() const {
		if (category == FileCategory::File) throw base::LogicError("Path is not a directory");

		if (type == FileType::Virtual) return vfs.listDirectory(path);
		std::vector<std::string> files;
		for (const auto& entry: std::filesystem::directory_iterator(path))
			files.push_back(entry.path().filename().string());
		return files;
	}

	base::OwningView getSimpleFileContent(const std::string& file_name) {
		std::ifstream file(file_name, std::ios::in | std::ios::binary);
		if (file.fail()) throw base::LogicError(std::string("file does not exist: ") + file_name);

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

	base::OwningView getSimpleVirtualFileContent(const std::string& file_name) {
		auto content = vfs.readFile(file_name);
		if (content.empty())
			throw base::LogicError(std::string("virtual file does not exist: ") + file_name);

		auto r_array = new byte[content.size()];
		std::ranges::copy(content, reinterpret_cast<char*>(r_array));

		return { r_array, content.size() };
	}

	base::HashT FilePath::customPerfectHash() const {
		static base::HashT                          next_hash = 0;
		static base::HashMap<FilePath, base::HashT> hash_map;

		// @Future: use atMaybe
		if (hash_map.contains(*this)) return hash_map.at(*this);

		auto hash = next_hash++;

		hash_map.put(*this, hash);
		return hash;
	}
}
