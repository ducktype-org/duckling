/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "file.hpp"
#include <base/maps.hpp>
#include <base/perfect_hash.hpp>
#include <base/exceptions.hpp>

#include <fstream>
#include <random>

namespace {
	std::filesystem::path
		random_name(const std::filesystem::path& prefix_path, const size_t name_len = 16) {
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
}

namespace fs {
	FilePath::ContentMap FilePath::to_content;

	FilePath FilePath::getDefaultTempPath() {
		static FilePath tempDirectoryPath
			= createTempFilePathObj(std::filesystem::temp_directory_path());
		return tempDirectoryPath;
	}

	std::filesystem::path FilePath::genTempPathInMe(const std::string& custom_name) const {
		if (!is_temporary) throw base::LogicError("Parent is not temporary");

		std::filesystem::path file_name;
		if (custom_name.empty())
			file_name = random_name(path);
		else {
			file_name = path / custom_name;
			if (exists(file_name))
				throw base::LogicError(
					"Cannot create a file/dir with name \"" + custom_name
					+ "\", because there already is a file/dir with this name in " + absolutePath()
				);
		}
		return file_name;
	}

	FilePath FilePath::createTempFilePathObj(const std::filesystem::path& path) {
		auto&& obj       = FilePath(path);
		obj.is_temporary = true;
		return obj;
	}

	FilePath FilePath::createTempFile(const std::string& content) {
		return getDefaultTempPath().createTempFileIn(content);
	}

	FilePath FilePath::createTempDirectory() {
		return getDefaultTempPath().createTempDirectoryIn();
	}

	FilePath FilePath::createTempDirectoryIn(const std::string& custom_name) const {
		auto&& new_temp_dir = genTempPathInMe(custom_name);
		create_directory(new_temp_dir);
		return createTempFilePathObj(new_temp_dir);
	}

	FilePath FilePath::createTempFileIn(
		const std::string& new_file_content, const std::string& custom_name
	) const {
		auto&&       new_temp_file = genTempPathInMe(custom_name);
		std::fstream temp_file(new_temp_file, std::ios::out | std::ios::app);
		temp_file << new_file_content;
		temp_file.close();
		return createTempFilePathObj(new_temp_file);
	}

	FileContent FilePath::getContent() const {
		if (to_content.contains(path)) {
			auto weak_content = to_content[path];
			if (weak_content.expired())
				to_content.erase(path);
			else
				return FileContent(weak_content.lock());
		}

		FileContent file_content(
			std::make_shared<base::OwningView>(getSimpleFileContent(path.c_str()))
		);

		to_content.put(path, file_content.content);

		return file_content;
	}

	cpp::result<FileContent, std::string> FilePath::getContentSafe() const {
		if (!exists(path)) {
			return cpp::fail(base::strConcat(
				"Error: cannot get content of file `", path, "` - file does not exist"
			));
		}
		return getContent();
	}

	std::string_view FilePath::strView() const { return path.c_str(); }

	FilePath FilePath::parentPath() const { return path.parent_path(); }

	std::string FilePath::absolutePath() const { return path; }

	std::string FilePath::name() const {
		if (isDirectory() && path.filename() == ".") return path.parent_path().filename();
		return path.filename();
	}

	bool FilePath::isDirectory() const noexcept { return is_directory(path); }

	std::chrono::file_clock::time_point FilePath::getModifyTime() const {
		return last_write_time(path);
	}

	bool FilePath::isFile() const noexcept { return !isDirectory(); }

	std::string FilePath::stem() const { return path.stem(); }

	std::string FilePath::extension() const { return path.extension(); }

	std::filesystem::directory_iterator FilePath::directoryIterator() const {
		return std::filesystem::directory_iterator(path);
	}

	base::OwningView getSimpleFileContent(const std::string& file_name) {
		std::ifstream file(file_name, std::ios::in | std::ios::binary);
		if (file.fail()) throw base::LogicError(std::string("file does not exist: ") + file_name);

		file.unsetf(std::ios::skipws);

		auto fpos = file.tellg();
		file.seekg(0, std::ios::end);
		std::streamoff fsize     = file.tellg() - fpos;
		usize          file_size = fsize;
		file.seekg(0, std::ios::beg);

		// should read full file:
		auto r_array = new byte[file_size];
		file.read(reinterpret_cast<char*>(r_array), std::streamsize(file_size));

		return { r_array, file_size };
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
