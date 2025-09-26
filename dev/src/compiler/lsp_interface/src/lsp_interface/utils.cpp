#include "utils.hpp"
#include "base/exceptions.hpp"
#include "base/str_utils.hpp"
#include "filesystem/file.hpp"

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

		auto file = fs::File(path);
		auto virtual_path = vRoot.getFilePath().join(path.uri());

		if (file.isFile()) {
			if (ext_is_ok(path.extension())) {
				(void) vRoot.createSubFile(file.getContent().view().stringView(), virtual_path.strView());
			}
			return;
		}

		if (file.isDirectory()) {
			for (const auto& sub_path: file.listFilePaths()) {
				(void) vRoot.createSubDirectory(virtual_path.strView());
				initFiles(sub_path, vRoot);
			}
			return;
		}

		CORE_UNREACHABLE()
	}

	void initModules(const fs::FilePath& path) {
		std::cout << "MODULES\n";
		auto file = fs::File(path);

		if (file.isFile()) {
			if (path.extension() == ".dmf") {
				compiler::frontend::createModuleTree(file);
			}
			return;
		}

		if (file.isDirectory()) {
			for (const auto& sub_path: file.listFilePaths()) {
				initModules(sub_path);
			}
			return;
		}

		CORE_UNREACHABLE()
	}
}
