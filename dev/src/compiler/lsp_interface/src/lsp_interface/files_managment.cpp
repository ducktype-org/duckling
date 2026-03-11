#include "files_managment.hpp"

#include <driver/incremental_utils/collect_input.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <global_state/packages.hpp>

#include <query_framework/external/api.hpp>

#include <unordered_set>

namespace lsp {
	using fs::File;
	using namespace compiler::frontend;

	namespace {
		std::vector<fs::FilePath> workspace_roots;

		bool hasMainModuleFile(const fs::FilePath& dir_path) {
			auto dir = fs::File(dir_path);
			if (!dir.isDirectory()) return false;

			auto module_file = dir_path.join(dir.name() + std::string(LANG_MODULE_FILE));
			return module_file.exists();
		}

		base::Optional<fs::File> getMainModuleFile(const fs::FilePath& dir_path) {
			auto dir = fs::File(dir_path);
			if (!dir.isDirectory()) return {};

			auto module_file = dir_path.join(dir.name() + std::string(LANG_MODULE_FILE));
			if (module_file.exists()) return fs::File(module_file);
			return {};
		}

		// ========================== Package related helpers ==========================

		void createRootModuleAndRegisterPackage(const fs::File& file) {
			auto module_id = createModuleTreeWithRandomPackageID(file);
			global_state::setters::addPackage(module_id);
			std::cerr << "Created package for " << file.getFilePath().strView() << "\n";
		}

		void removeModuleAndUnregisterPackage(ModuleID module_id) {
			;
			if (not getModuleRef(module_id)->getParentModule().has_value())
				global_state::setters::removePackage(module_id);

			ModuleTreeModifier::removeModuleRecursive(
				GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					module_id
				)
			);
		}

		// ========================== Adding file to module tree ==========================

		base::Optional<base::Ref<ModuleTree>> findModuleForPath(
			const fs::File& expected_module_directory
		) {
			const auto& dir = expected_module_directory;
			if (!dir.isDirectory()) return {};
			if (!hasMainModuleFile(dir.getFilePath())) return {};

			auto module_file_name = dir.name() + std::string(LANG_MODULE_FILE);
			auto module_file_path = dir.getFilePath().join(module_file_name);

			auto source_files = SourceFile::getSourceFilesFromFile(fs::File(module_file_path));
			if (source_files.empty()) return {};

			auto module_id = source_files.back()->getModule().illegalAccess().getID();
			return GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module_id
			);
		}

		void addFileToModuleTree(const fs::File& file) {
			auto extension  = file.extension();
			auto parent_dir = file.getFilePath().parentPath();


			if (extension == LANG_SOURCE_FILE) {
				auto module_ref_opt = findModuleForPath(parent_dir);
				if_opt_some(module_ref_opt, module_ref)
					ModuleTreeModifier::addSourceFile(module_ref, file);
				return;
			}

			if (extension == LANG_MODULE_FILE) {
				base::Optional<base::Ref<ModuleTree>> module_ref_opt;
				base::StrID                           stem_id(file.stem().c_str());
				if (file.stem() == parent_dir.name()) {
					// Case when we have a/a.dmf submodule structure.
					module_ref_opt = findModuleForPath(parent_dir.parentPath());
				} else {
					// Case when we have b/a.dmf
					module_ref_opt = findModuleForPath(parent_dir);
				}

				if_opt_some(module_ref_opt, module_ref) {
					auto submodule
						= ModuleTreeBuilder::create(file, module_ref->getPackageID().strView());
					ModuleTreeModifier::addSubmodule(module_ref, submodule);
				}
				if_opt_none(module_ref_opt) { createRootModuleAndRegisterPackage(file); }
				return;
			}
		}

		// ========================== Removing a file from module tree ==========================
		//

		void removeFileFromModuleTree(const fs::File& file) {
			;

			auto                         source_files = SourceFile::getSourceFilesFromFile(file);
			std::unordered_set<ModuleID> modules_to_remove;

			for (auto& source_file: source_files) {
				auto module_id  = source_file->getModule().illegalAccess().getID();
				auto module_ref = getModuleRef(module_id);

				bool is_main_source_file = false;
				if (module_ref->hasMainSourceFile()) {
					auto main_source_file = module_ref->getMainSourceFile().illegalAccess().getID();
					is_main_source_file   = (main_source_file == source_file->getFileID());
				}

				if (is_main_source_file) modules_to_remove.insert(module_id);
			}

			for (auto& source_file: source_files) {
				auto module_id = source_file->getModule().illegalAccess().getID();
				if (modules_to_remove.contains(module_id)) continue;

				ModuleTreeModifier::removeSourceFileFromStorage(source_file);
			}

			for (const auto& module_id: modules_to_remove)
				removeModuleAndUnregisterPackage(module_id);
		}

		/**
		 * @brief Recursively copies all duckling files from the physical file
		 *  system to the virtual file system, starting from the given path.
		 *
		 * The start path have to exists.
		 *
		 * @param start_physical_path Start physical path in the filesystem to copy from.
		 */
		void copyDucklingFilesToVirtual(const fs::FilePath& start_physical_path) {
			auto        file         = fs::File(start_physical_path);
			const auto& path         = start_physical_path;
			const auto& virtual_path = path.toVirtualPath();

			if (file.isFile()) {
				if (path.extension() == LANG_MODULE_FILE || path.extension() == LANG_SOURCE_FILE) {
					fs::FileManager::createVirtualFile(
						virtual_path, file.getContent().view().stringView(), true
					);
				}
			}

			if (file.isDirectory()) {
				if (!virtual_path.exists()) fs::FileManager::createVirtualFolder(virtual_path);

				for (const auto& sub_path: file.listFilePaths())
					copyDucklingFilesToVirtual(sub_path);
			}
		}
	}

	void addWorkspace(const fs::FilePath& absolute_physical_path) {
		if (std::ranges::find(workspace_roots, absolute_physical_path) != workspace_roots.end())
			return;

		workspace_roots.push_back(absolute_physical_path);
		std::cerr << "Registered workspace root: " << absolute_physical_path.strView() << "\n";
	}

	void openFile(const fs::FilePath& absolute_physical_path) {
		if (absolute_physical_path.toVirtualPath().exists()) return;

		// Get the correct workspace root for the file.
		auto it = std::ranges::find_if(workspace_roots, [&](const fs::FilePath& root) {
			return absolute_physical_path.strView().starts_with(root.strView());
		});
		if (it == workspace_roots.end()) return;
		fs::FilePath workspace_root = *it;


		fs::FilePath package_root_dir = absolute_physical_path;

		// Get the directory to start searching for the package root.
		auto current = fs::File(absolute_physical_path).isDirectory()
		                 ? absolute_physical_path
		                 : absolute_physical_path.parentPath();

		while (current.exists()) {
			if (hasMainModuleFile(current)) package_root_dir = current;
			if (current == workspace_root) break;
			current = current.parentPath();
		}

		if (package_root_dir.toVirtualPath().exists()) {
			// If the package is already loaded, it means that the file is a new file in an already
			// loaded package, so we just create it in the virtual file system.
			lsp::addFile(absolute_physical_path, "");
		} else {
			// If the package is not loaded, we need to load it first.
			copyDucklingFilesToVirtual(package_root_dir);
			createRootModuleAndRegisterPackage(package_root_dir.toVirtualPath());
		}
	}

	void addFile(const fs::FilePath& absolute_physical_path, const std::string& content) {
		auto virtual_path = absolute_physical_path.toVirtualPath();
		if (virtual_path.exists()) return;

		auto file = fs::FileManager::createVirtualFile(virtual_path, content);

		addFileToModuleTree(file);

		auto new_inputs = compiler::driver::collectInputDataFromGlobalPackagesFromCurrentMetadata();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}

	void removeFile(const fs::FilePath& absolute_physical_path) {
		auto virtual_path = absolute_physical_path.toVirtualPath();
		if (!virtual_path.exists()) return;

		auto file = fs::File(virtual_path);
		removeFileFromModuleTree(file);

		fs::FileManager::deleteFile(file);

		auto new_inputs = compiler::driver::collectInputDataFromGlobalPackagesFromCurrentMetadata();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}

	void updateFileContent(const fs::FilePath& absolute_physical_path, const std::string& content) {
		auto file_path = absolute_physical_path.toVirtualPath();
		auto file      = fs::File(file_path);

		auto source_files = SourceFile::getSourceFilesFromFile(file);


		std::vector<query::external::InputData> previous_inputs;

		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			compiler::driver::collectQueryInputsFromPst(pst, previous_inputs);
		}

		file.writeToFile(content);
		ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;

		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			compiler::driver::collectQueryInputsFromPst(pst, new_inputs);
		}

		// std::vector<query::external::InputData> invalidated_inputs;
		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
		// for (const auto& input: invalidated_inputs) {
		// 	std::cerr << "Invalidated input with QueryID: " << input.q_id.getData().name
		// 			  << " and hash: " << input.hash << "\n";
		// }
	}
}
