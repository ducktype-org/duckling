/** 
 * @file file.hpp
 * @author Kacper Chętkowski (kacper.chetkowski@gmail.com)
 */

#include "file.hpp"
#include <base/exceptions.hpp>

#include <fstream>
#include <iterator>
#include <utility>

namespace fs {
	FilePath::ContentMap FilePath::to_content;

	FilePath::FilePath(const std::filesystem::path& path): path(std::filesystem::absolute(path)) {}

	FilePath::FilePath(const FilePath& oth): path(oth.path) {}

	FileContent FilePath::getContent() const {
		if (to_content.contains(path)) {
			auto weak_content = to_content[path];
			if (weak_content.expired()) {
				to_content.erase(path);
			}
			else {
				return weak_content.lock();
			}
		}

		FileContent file_content(
			std::make_shared<base::OwningView>(
				std::move(getSimpleFileContent(path.c_str()))
			)
		);
		
		to_content.put(path, file_content.content);
		
		return file_content;
	}

	result<FileContent, std::string> FilePath::getContentSafe() const {
		if (!std::filesystem::exists(path)) {
			return fail(base::strConcat("Error: cannot get content of file `", path, "` - file does not exist"));
		}
		return getContent();
	}

	std::string_view FilePath::strView() const {
		return path.c_str();
	}

	FilePath FilePath::parentPath() const {
		return path.parent_path();
	}

	base::OwningView getSimpleFileContent(const std::string& file_name) {
		std::ifstream file(file_name, std::ios::in | std::ios::binary);
		if (file.fail()) {
			throw base::LogicError(std::string("file does not exist: ") + file_name);
		}

		file.unsetf(std::ios::skipws);

		auto fpos = file.tellg();
		file.seekg(0, std::ios::end);
		std::streamoff fsize = file.tellg() - fpos;
		size_t file_size = fsize;
		file.seekg(0, std::ios::beg);

		// should read full file:
		auto r_array = new uint8_t[file_size];
		file.read(reinterpret_cast<char*>(r_array), file_size);

		return {r_array, file_size};
	}
}