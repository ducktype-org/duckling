#include "file_changed.hpp"

#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>

#include <query_framework/external/api.hpp>

#include <algorithm>
#include <unordered_set>

namespace compiler::driver {
	std::vector<query::external::InputData> collectInputDataFromGlobalPackagesFromCurrentMetadata();
}

namespace lsp {
	using fs::File;

	namespace {
		/**
		 * @brief Helper to get file from virtual root, creating it if it doesn't exist.
		 */
		fs::File getFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path) {
			if (virtual_root.getFilePath().join(path).exists())
				return { virtual_root.getFilePath().join(path) };
			else {
				fs::FileManager::createVirtualFile(virtual_root.getFilePath().join(path), "");
				return { virtual_root.getFilePath().join(path) };
			}
		}

		fs::File writeToFileFromVirtualRoot(
			const fs::File& virtual_root, const std::string& path, const std::string& content
		) {
			auto file = getFileFromVirtualRoot(virtual_root, path);
			file.writeToFile(content);
			return file;
		}

		std::vector<query::external::InputData> collectAllCurrentInputs() {
			return compiler::driver::collectInputDataFromGlobalPackagesFromCurrentMetadata();
		}

		void collectQueryInputsFromPst(
			CRef<pst::PST<>> pst_ref, std::vector<query::external::InputData>& out
		) {
			auto root = pst_ref->getRootElement();
			if (auto maybe_root = root.illegalAccess()) {
				auto root_unlocked = maybe_root.value();
				out.emplace_back(
					pst::internal::PSTAccessSideInput::getID(), root_unlocked->getHash()
				);

				auto elems = pst::viewAllSubTreeElements(root);
				for (auto& el: elems)
					if (auto maybe_elem = el.illegalAccess()) {
						auto ptr = maybe_elem.value();
						out.emplace_back(pst::internal::PSTAccessSideInput::getID(), ptr->getHash());
					}
			}
		}

		base::Optional<base::Ref<compiler::frontend::ModuleTree>>
		findModuleForDirectory(const fs::File& directory) {
			using namespace compiler::frontend;

			if (!directory.isDirectory()) return {};

			auto main_file_name = directory.name() + std::string(LANG_MODULE_FILE);
			auto main_file_path = directory.getFilePath().join(main_file_name);
			if (!main_file_path.exists()) return {};

			auto source_files = SourceFile::getSourceFilesFromFile(fs::File(main_file_path));
			if (source_files.empty()) return {};

			auto module_id = source_files.front()->getModule().illegalAccess().getID();
			return getModuleRef(module_id);
		}

		base::Optional<base::Ref<compiler::frontend::ModuleTree>> findOwningModule(
			const fs::File& virtual_root, const fs::File& file
		) {
			auto root_path    = virtual_root.getFilePath();
			auto current_path = file.getFilePath().parentPath();

			while (current_path.exists()) {
				auto current_dir = fs::File(current_path);
				if (current_dir.isDirectory()) {
					auto maybe_module = findModuleForDirectory(current_dir);
					if (maybe_module.has_value()) return maybe_module;
				}

				if (current_path == root_path) break;

				auto next_path = current_path.parentPath();
				if (next_path == current_path) break;
				current_path = next_path;
			}

			return {};
		}

		void addFileToModuleTree(const fs::File& virtual_root, const fs::File& file) {
			using namespace compiler::frontend;

			auto maybe_module = findOwningModule(virtual_root, file);
			if (!maybe_module.has_value()) return;

			auto module_ref = maybe_module.value();

			auto extension = file.extension();
			if (extension == LANG_SOURCE_FILE) {
				ModuleTreeModifier::addSourceFile(module_ref, file);
				return;
			}

			if (extension == LANG_MODULE_FILE) {
				base::StrID stem_id(file.stem().c_str());

				if (stem_id == module_ref->getName()) {
					if (!module_ref->hasMainSourceFile())
						ModuleTreeModifier::setMainSourceFile(module_ref, file);
					return;
				}

				auto submodule = ModuleTreeBuilder::create(file, module_ref->getPackageID().strView());
				ModuleTreeModifier::addSubmodule(module_ref, submodule);
				return;
			}

			ModuleTreeModifier::addOtherFile(module_ref, file);
		}
	}

	fs::File createFileFromVirtualRoot(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);
		file.writeToFile(content);

		addFileToModuleTree(virtual_root, file);

		auto new_inputs = collectAllCurrentInputs();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
		return file;
	}

	void updateFileContent(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file_path = virtual_root.getFilePath().join(path);
		if (!file_path.exists()) return;

		auto file = fs::File(file_path);

		auto source_files = compiler::frontend::SourceFile::getSourceFilesFromFile(file);

		file = writeToFileFromVirtualRoot(virtual_root, path, content);
		compiler::frontend::ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;
		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			collectQueryInputsFromPst(pst, new_inputs);
		}

		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}

	void removeFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path) {
		auto file_path = virtual_root.getFilePath().join(path);
		if (!file_path.exists()) return;

		auto file = fs::File(file_path);
		auto source_files    = compiler::frontend::SourceFile::getSourceFilesFromFile(file);

		std::unordered_set<compiler::frontend::ModuleID> modules_to_remove;

		for (auto& source_file: source_files) {
			auto module_id = source_file->getModule().illegalAccess().getID();
			auto module_ref = compiler::frontend::getModuleRef(module_id);

			bool is_main_source_file = false;
			if (module_ref->hasMainSourceFile()) {
				auto main_source_file = module_ref->getMainSourceFile().illegalAccess().getID();
				is_main_source_file = (main_source_file == source_file->getFileID());
			}

			if (is_main_source_file)
				modules_to_remove.insert(module_id);
		}

		for (auto& source_file: source_files) {
			auto module_id = source_file->getModule().illegalAccess().getID();
			if (modules_to_remove.contains(module_id)) continue;

			compiler::frontend::ModuleTreeModifier::removeSourceFileFromStorage(source_file);
		}

		for (const auto& module_id: modules_to_remove)
			compiler::frontend::ModuleTreeModifier::removeModuleRecursive(
				compiler::frontend::getModuleRef(module_id)
			);

		// if (source_files.empty()) {
		// 	auto maybe_owner_module = findOwningModule(virtual_root, file);
		// 	if (maybe_owner_module.has_value()) {
		// 		auto owner_module = maybe_owner_module.value();

		// 		auto ext_id = base::StrID(file.extension().c_str());
		// 		if (owner_module->getOtherFiles().contains(ext_id)) {
		// 			const auto& files = owner_module->getOtherFiles().at(ext_id);
		// 			auto it           = std::ranges::find_if(files, [&](const fs::File& existing) {
		// 				return existing.getFilePath() == file.getFilePath();
		// 			});

		// 			if (it != files.end())
		// 				compiler::frontend::ModuleTreeModifier::removeOtherFile(owner_module, file);
		// 		}
		// 	}
		// }

		fs::FileManager::deleteFile(file);

		auto new_inputs = collectAllCurrentInputs();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}
}
