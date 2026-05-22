#include "files_managment.hpp"

#include <driver/incremental_utils/collect_input.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/source_position_locked.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>
#include <global_state/packages.hpp>

#include <query_framework/external/api.hpp>

#include <unordered_set>

namespace lsp {
	using fs::File;
	using namespace compiler::frontend;

	namespace {
		// Global state for registered workspace roots.
		// We use it to know where to stop when we walk up the filesystem looking for the package root.
		std::vector<fs::FilePath> workspace_roots;

		// ========================== Filesystem related helpers ==========================

		/**
		 * @brief For a given directory path, checks if it contains a main module file
		 * (dir/dir.dmf). If the directory does not exist, it throws an exception.
		 */
		bool hasDirectoryMainModuleFile(const fs::FilePath& dir_path) {
			auto dir         = fs::File(dir_path);
			auto module_file = dir_path.join(dir.name() + std::string(LANG_MODULE_FILE));
			return module_file.exists();
		}

		/**
		 * @brief For a given directory path, returns the path to the main module file (dir/dir.dmf)
		 * if it exists.
		 */
		base::Optional<fs::File> getDirectoryMainModuleFile(const fs::FilePath& dir_path) {
			if (hasDirectoryMainModuleFile(dir_path))
				return dir_path.join(dir_path.name() + std::string(LANG_MODULE_FILE));
			return {};
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
				if (path.extension() == LANG_MODULE_FILE) {
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

		// ========================== Package related helpers ==========================

		/**
		 * @brief Creates a new root module for the given file and registers it as a package.
		 */
		void createRootModuleAndRegisterPackage(const fs::File& file) {
			auto module_id = createModuleTreeWithRandomPackageID(file);
			global_state::setters::addPackage(module_id);
			std::cerr << "Created package for " << file.getFilePath().strView() << "\n";
		}

		/**
		 * @brief Recursively removes a module and all its submodules from the module tree.
		 * If the removed module is a root module, it is unregistered.
		 */
		void removeModuleAndUnregisterPackage(ModuleID module_id) {
			if (getModuleRef(module_id)->getParentModule().empty())
				global_state::setters::removePackage(module_id);

			ModuleTreeModifier::removeModuleRecursive(
				GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
					module_id
				)
			);
		}

		// ========================== Adding file to module tree ==========================

		/**
		 * @brief For a given file path, finds the corresponding module in the module tree if
		 * it exists.
		 */
		base::Optional<base::Ref<ModuleTree>> findModuleForFile(const fs::File& file) {
			auto source_files = SourceFile::getSourceFilesFromFile(file);
			if (source_files.empty()) return {};
			auto module_id = source_files.back()->getModule().illegalAccess().getID();
			return GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module_id
			);
		}

		/**
		 * @brief For a given directory path, finds the corresponding module in the module tree if
		 * it exists.
		 */
		base::Optional<base::Ref<ModuleTree>> findModuleForDirectoryPath(const fs::File& dir) {
			if (not hasDirectoryMainModuleFile(dir.getFilePath())) return {};
			auto module_file_path = getDirectoryMainModuleFile(dir.getFilePath()).value();
			return findModuleForFile(fs::File(module_file_path));
		}

		base::Optional<base::Ref<ModuleTree>> findModuleForFileOrDirPath(const fs::File& file) {
			if (file.isDirectory()) return findModuleForDirectoryPath(file);
			if (file.isFile()) return findModuleForFile(file);
			return {};
		}

		/**
		 * @brief This checks if the given file should
		 * become a new module in the module tree and if it should
		 * be a submodule or a new root module.
		 */
		void addFileToModuleTree(const fs::File& file) {
			auto extension  = file.extension();
			auto parent_dir = fs::File(file.getFilePath().parentPath());

			if (extension == LANG_MODULE_FILE) {
				base::Optional<base::Ref<ModuleTree>> parent_module_ref_opt;
				base::StrID                           stem_id(file.stem().c_str());
				if (file.stem() == parent_dir.name()) {
					// Case when we have a/a.dmf submodule structure.
					auto parent_dir_parent = fs::File(parent_dir.getFilePath().parentPath());
					parent_module_ref_opt  = findModuleForDirectoryPath(parent_dir_parent);
				} else {
					// Case when we have b/a.dmf
					parent_module_ref_opt = findModuleForDirectoryPath(parent_dir);
				}

				if_opt_some(parent_module_ref_opt, module_ref) {
					std::cerr << "Adding new file to already loaded package: "
							  << file.getFilePath().strView() << "\n";
					auto submodule = ModuleTreeBuilder::create(
						file, module_ref->getPackage().illegalAccess().getID()
					);
					ModuleTreeModifier::addSubmodule(module_ref, submodule);
				}
				if_opt_none(parent_module_ref_opt) { createRootModuleAndRegisterPackage(file); }
				return;
			}
		}

		/**
		 * @brief Scans for the children of the newly added module
		 * and adds them as submodules if needed.
		 *
		 * This is for a situation, where we add a file like /a/a.dmf
		 * and there are other files in the /a directory, like /a/b.dmf or /a/c/c.dmf,
		 * that should become submodules of /a/a.dmf.
		 */
		void checkForNewSubmodules(const fs::File& file) {
			auto parent_dir = fs::File(file.getFilePath().parentPath());
			if (parent_dir.stem() != file.stem()) return;

			if_opt_none(findModuleForDirectoryPath(parent_dir)) return;
			auto parent_module_ref = findModuleForDirectoryPath(parent_dir).value();


			for (auto& file_path: parent_dir.listFilePaths()) {
				auto module_ref_opt = findModuleForFileOrDirPath(fs::File(file_path));
				if_opt_none(module_ref_opt) {
					// There are no submodules for this file (single file module or module tree)
					std::cerr << "Adding new submodule " << file_path.strView()
							  << " to parent module " << parent_module_ref->getName().strView()
							  << "\n";
					auto submodule = ModuleTreeBuilder::create(
						fs::File(file_path), parent_module_ref->getPackage().illegalAccess().getID()
					);
					ModuleTreeModifier::addSubmodule(parent_module_ref, submodule);
				}
				if_opt_some(module_ref_opt, submodule) {
					// If there is a module for this file and it's a package root,
					// we need to move it under the new parent module.
					if (submodule->getParentModule().empty()) {
						// Is a package
						std::cerr << "Adding existing submodule " << submodule->getName().strView()
								  << " to new parent module "
								  << parent_module_ref->getName().strView() << "\n";
						ModuleTreeModifier::changePackageID(
							submodule, parent_module_ref->getPackage().illegalAccess().getID()
						);
						ModuleTreeModifier::addSubmodule(parent_module_ref, submodule);

						global_state::setters::removePackage(submodule->getModuleID());
					}
				}
			}
		}

		// ========================== Removing a file from module tree ==========================

		/**
		 * @brief Removes files and modules from the module tree. If the removed module was a
		 * package, it is unregistered.
		 */
		void removeFileFromModuleTree(const fs::File& file) {
			auto source_files = SourceFile::getSourceFilesFromFile(file);

			for (auto& source_file: source_files) {
				auto module_id  = source_file->getModule().illegalAccess().getID();
				auto module_ref = getModuleRef(module_id);

				CORE_ASSERT(
					module_ref->hasMainSourceFile(),
					"Modules should have main source files, since module_id is obtained from "
					"source file"
				);
				CORE_ASSERT(
					module_ref->getMainSourceFile().illegalAccess().getID()
						== source_file->getFileID(),
					"Module's main source file should be the same as the source file we are trying "
					"to remove"
				);

				removeModuleAndUnregisterPackage(module_id);
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
		auto current = absolute_physical_path.parentPath();

		while (not current.empty()) {
			if (current == workspace_root) break;
			if (hasDirectoryMainModuleFile(current))
				package_root_dir = current;
			else
				break;

			current = current.parentPath();
		}

		if (package_root_dir.toVirtualPath().exists()) {
			// If the package is already loaded, it means that the file is a new file in an already
			// loaded package, so we just create it in the virtual file system.
			lsp::addFile(absolute_physical_path);
		} else {
			// If the package is not loaded, we need to load it first.
			copyDucklingFilesToVirtual(package_root_dir);
			createRootModuleAndRegisterPackage(package_root_dir.toVirtualPath());
		}
	}

	void addFile(const fs::FilePath& absolute_physical_path) {
		auto virtual_path = absolute_physical_path.toVirtualPath();
		if (virtual_path.exists()) return;

		copyDucklingFilesToVirtual(absolute_physical_path);
		auto file = fs::File(virtual_path);

		addFileToModuleTree(file);
		checkForNewSubmodules(file);

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

	void removeFileOrDirectory(const fs::FilePath& absolute_physical_path) {
		auto virtual_path = absolute_physical_path.toVirtualPath();
		if (!virtual_path.exists()) return;

		auto file = fs::File(virtual_path);

		if (file.isFile()) {
			removeFile(absolute_physical_path);
			return;
		}

		if (file.isDirectory()) {
			for (const auto& sub_path: file.listFilePaths()) {
				// sub_path is a virtual path — convert back to physical for the recursive call
				removeFileOrDirectory(sub_path.toPhysicalPath());
			}
			fs::FileManager::deleteFolder(file, true);
		}
	}

	void updateFileContent(const fs::FilePath& absolute_physical_path, const std::string& content) {
		std::cerr << "Updating content of file: " << absolute_physical_path.strView() << "\n";
		auto file_path = absolute_physical_path.toVirtualPath();
		auto file      = fs::File(file_path);

		auto source_files = SourceFile::getSourceFilesFromFile(file);


		std::vector<query::external::InputData> previous_inputs;
		previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());

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
