/**
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "file.hpp"
#include "base/maps.hpp"
#include "base/perfect_hash.hpp"
#include <base/exceptions.hpp>

#include <fstream>
#include <iterator>

namespace fs {
	FilePath::ContentMap FilePath::to_content;

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
		if (!std::filesystem::exists(path)) {
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
		static base::HashT next_hash = 0;
		static base::HashMap<FilePath, base::HashT> hash_map;
		
		// @Future: use atMaybe
		if (hash_map.contains(*this)) {
			return hash_map.at(*this);
		}

		auto hash = next_hash++;

		hash_map.put(*this, hash);
		return hash;
	}
}
