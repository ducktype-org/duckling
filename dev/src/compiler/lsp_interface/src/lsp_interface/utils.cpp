#include "utils.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include "filesystem/file.hpp"
#include "filesystem/file_path.hpp"
#include <query_framework/utils/with_context_do.hpp>

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

	fs::FilePath initFiles(const fs::FilePath& path, const fs::File& virtual_root) {
		const fs::FilePath slash        = "/";
		auto               file         = fs::File(slash / path);
		auto               virtual_path = virtual_root.getFilePath().join(path);

		if (file.isFile()) {
			if (ext_is_ok(path.extension())) {
				fs::FileManager::createVirtualFile(
					virtual_path, file.getContent().view().stringView(), true
				);
			}
			return virtual_path;
		}

		if (file.isDirectory()) {
			if (!virtual_path.exists()) fs::FileManager::createVirtualFolder(virtual_path);
			for (const auto& sub_path: file.listFilePaths()) {
				fs::FilePath relative_sub_path
					= sub_path.strView().substr(1, sub_path.strView().length());
				initFiles(relative_sub_path, virtual_root);
			}
			return virtual_path;
		}

		CORE_UNREACHABLE();
	}

	void initModules(const fs::FilePath& path) {
		auto vfile = fs::File(path);

		if (vfile.isFile()) {
			if (path.extension() == ".dmf") compiler::frontend::createModuleTreeWithRandomPackageID(vfile);
			return;
		}

		if (vfile.isDirectory()) {
			for (const auto& sub_path: vfile.listFilePaths()) initModules(sub_path);
			return;
		}

		CORE_UNREACHABLE();
	}

	void initPSTs(const fs::FilePath& path) {
		auto vfile = fs::File(path);

		if (vfile.isFile()) {
			if (ext_is_ok(path.extension())) {
				query::utils::withContextDo([&vfile](query::Context& ctx) {
					auto src_files = compiler::frontend::SourceFile::getSourceFilesfromFile(vfile);
					for (auto& src_file: src_files)
						ctx.query<compiler::frontend::QueryFilePST>(src_file->getFileID());
				});
			}
			return;
		}

		if (vfile.isDirectory()) {
			for (const auto& sub_path: vfile.listFilePaths()) initPSTs(sub_path);
			return;
		}

		CORE_UNREACHABLE();
	}

	void putFile(const fs::File& virtual_root, std::string path, std::string content) {
		if (!virtual_root.getFilePath().join(path).exists())
			fs::FileManager::createVirtualFile(virtual_root.getFilePath().join(path), "");

		auto file = fs::File(virtual_root.getFilePath().join(path));
		file.writeToFile(content);

		compiler::frontend::ModuleTreeModifier::fileModified(file);
		query::utils::withContextDo([&file](query::Context& ctx) {
			auto src_files = compiler::frontend::SourceFile::getSourceFilesfromFile(file);
			for (auto& src_file: src_files)
				ctx.query<compiler::frontend::QueryFilePST>(src_file->getFileID());
		});
	}
}
