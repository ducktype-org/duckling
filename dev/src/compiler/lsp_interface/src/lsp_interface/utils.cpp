#include "utils.hpp"
#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>
#include "filesystem/file.hpp"
#include "filesystem/file_path.hpp"

#include <frontend/module_tree/module_tree.hpp>

#define ext_is_ok(ext) ext == ".dmf" || ext == ".duckling" || ext == ".dl" || ext == ".rift"

namespace lsp {
	std::string jsonList(const std::vector<std::string>& list) {
		std::string result = "[";
		for (const std::string& str: list) result += str + ",";
		if (result[result.length() - 1] == ',') result.pop_back();
		result += "]";

		return result;
	}

	std::string jsonDict(const std::map<std::string, std::string>& dict) {
		std::string result = "{";
		for (const auto& pair: dict) {
			const auto& key   = pair.first;
			const auto& value = pair.second;
			result += "\"";
			result += key;
			result += "\":";
			result += value;
			result += ",";
		}
		if (result[result.length() - 1] == ',') result.pop_back();
		result += "}";

		return result;
	}

	void initFiles(const fs::FilePath& path, const fs::File& vRoot) {
		std::cout << "PATH: " << path.strView() << "\n";
		const fs::FilePath slash = "/"; 
		auto file = fs::File(slash / path);
		auto virtual_path = vRoot.getFilePath().join(path);
		std::cout << "VPATH: " << virtual_path.strView() << "\n";

		if (file.isFile()) {
			if (ext_is_ok(path.extension())) {
				fs::FileManager::createVirtualFile(virtual_path, file.getContent().view().stringView(), true);
				//(void) vRoot.createSubFile(file.getContent().view().stringView(), virtual_path.strView());
			}
			return;
		}

		if (file.isDirectory()) {
			if (!virtual_path.exists())
				fs::FileManager::createVirtualFolder(virtual_path);
			for (const auto& sub_path: file.listFilePaths()) {
				fs::FilePath relative_sub_path = sub_path.strView().substr(1, sub_path.strView().length());
				//std::cout << "SUBPATH: " << relative_sub_path.strView() << "\n";
				//(void) vRoot.createSubDirectory(virtual_path.strView());
				initFiles(relative_sub_path, vRoot);
			}
			return;
		}

		CORE_UNREACHABLE();
	}

	void initModules(const fs::FilePath& path) {
		std::cout << "MODULES\n";
		auto vfile = fs::File(path);

		if (vfile.isFile()) {
			if (path.extension() == ".dmf") {
				compiler::frontend::createModuleTree(vfile);
			}
			return;
		}

		if (vfile.isDirectory()) {
			for (const auto& sub_path: vfile.listFilePaths()) {
				initModules(sub_path);
			}
			return;
		}

		CORE_UNREACHABLE();
	}
}
