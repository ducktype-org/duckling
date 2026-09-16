#include <driver/incremental_utils/collect_input.hpp>
#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>
#include <frontend/pst_parser/source_position_locked.hpp>
#include <global_state/packages.hpp>
#include <lsp_interface/compiler_files_management.hpp>

#include <query_framework/external/api.hpp>

#include <iostream>

namespace duck_ls {

	using namespace compiler::frontend;

	namespace {
		/**
		 * @brief Whether `dir` holds the `dir/dir.dk` file that makes it a module root.
		 */
		bool hasDirectoryMainModuleFile(const fs::FilePath& dir_path) {
			if (!dir_path.exists()) return false;
			return dir_path.join(dir_path.name() + std::string(LANG_MODULE_FILE)).exists();
		}

		/**
		 * @brief Collects the query inputs of every source file registered for `file`.
		 */
		void collectInputsFor(const fs::File& file, std::vector<query::external::InputData>& out) {
			for (auto& source_file: SourceFile::getSourceFilesFromFile(file))
				compiler::driver::collectQueryInputsFromPst(source_file->getPST(), out);
		}

		/**
		 * @brief Finds the module whose main source file is `file`.
		 */
		base::Optional<base::Ref<ModuleTree>> findModuleForFile(const fs::File& file) {
			auto source_files = SourceFile::getSourceFilesFromFile(file);
			if (source_files.empty()) return {};

			auto module_id = source_files.back()->getModule().illegalAccess().getID();
			return GetModuleID_Functor::getModRefUseOnlyWhenYouKnowWhatYouAreDoingThisCanModifyInput(
				module_id
			);
		}
	}

	base::StrID packageIdForRoot(const fs::FilePath& package_root) {
		return base::StrID(package_root.absolute().genericString());
	}

	CompilerFilesManagement::CompilerFilesManagement(base::Ref<ServerSession> session):
		  session(session) {}

	void CompilerFilesManagement::addWorkspace(const fs::FilePath& root) {
		session->addWorkspaceRoot(root);
	}

	base::Optional<fs::FilePath> CompilerFilesManagement::findPackageRoot(
		const fs::FilePath& path
	) const {
		auto workspace_root = session->workspaceRootFor(path);
		if (workspace_root.empty()) return {};

		base::Optional<fs::FilePath> package_root;

		for (auto current = path.parentPath(); !current.empty(); current = current.parentPath()) {
			if (!hasDirectoryMainModuleFile(current)) break;
			package_root = current;
			if (current == workspace_root.value()) break;
		}

		return package_root;
	}

	void CompilerFilesManagement::loadPackage(const fs::FilePath& package_root) {
		auto resolver = [this](const fs::FilePath& disk_path) -> fs::File {
			auto cache_twin = session->cachePath(disk_path);
			if (cache_twin.exists()) return cache_twin;
			return fs::File(disk_path);
		};

		auto module_id = createModuleTreeFromFS(
			fs::File(package_root), packageIdForRoot(package_root), resolver
		);
		global_state::setters::addPackage(module_id);

		std::cerr << "duck_ls: loaded package " << package_root.strView() << "\n";
	}

	void CompilerFilesManagement::reloadPackageOwning(const fs::FilePath& path) {
		auto package_root = findPackageRoot(path);
		if (package_root.empty()) return;

		// Tear the package down and walk it again: rebuilding is affordable for watcher events,
		// and incremental structural edits are where the subtle bugs live.
		auto root_module_file
			= package_root.value().join(package_root.value().name() + std::string(LANG_MODULE_FILE));
		auto cached_root = session->cachePath(root_module_file);
		auto root_file = cached_root.exists() ? fs::File(cached_root) : fs::File(root_module_file);

		if_opt_some(findModuleForFile(root_file), module_ref) {
			global_state::setters::removePackage(module_ref->getModuleID());
			ModuleTreeModifier::removeModuleRecursive(module_ref);
		}

		loadPackage(package_root.value());

		auto new_inputs = compiler::driver::collectInputDataFromGlobalPackagesFromCurrentMetadata();
		query::external::invalidateQueries(std::move(new_inputs), {}, {});
	}

	bool CompilerFilesManagement::swapMainSourceFile(
		const fs::FilePath& path, const fs::File& replacement
	) {
		auto current_file = fs::File(path);
		auto module_ref   = findModuleForFile(current_file);
		if (module_ref.empty()) return false;

		// The old PST must still exist while its inputs are collected.
		std::vector<query::external::InputData> previous_inputs;
		previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());
		collectInputsFor(current_file, previous_inputs);

		ModuleTreeModifier::removeMainSourceFile(module_ref.value());
		ModuleTreeModifier::setMainSourceFile(module_ref.value(), replacement);

		std::vector<query::external::InputData> new_inputs;
		collectInputsFor(replacement, new_inputs);

		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
		return true;
	}

	void CompilerFilesManagement::openDocument(const fs::FilePath& path, std::string_view text) {
		auto cache_twin = session->cachePath(path);
		fs::FileManager::createVirtualFile(cache_twin, text, true);

		if (swapMainSourceFile(path, fs::File(cache_twin))) return;

		// Nothing is registered for this path yet: either the package is not loaded, or the file
		// is new to a package that is. Both are answered by walking the package from disk, with
		// the cache twin already in place so the walk picks it up.
		auto package_root = findPackageRoot(path);
		if (package_root.empty()) {
			std::cerr << "duck_ls: no package root for " << path.strView() << "\n";
			return;
		}

		reloadPackageOwning(path);
	}

	void CompilerFilesManagement::updateDocument(const fs::FilePath& path, std::string_view text) {
		auto cache_twin = session->cachePath(path);
		if (!cache_twin.exists()) {
			openDocument(path, text);
			return;
		}

		auto file = fs::File(cache_twin);

		std::vector<query::external::InputData> previous_inputs;
		previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());
		collectInputsFor(file, previous_inputs);

		// Content first, then the modification notice: the content cache asserts the content has
		// not moved under it.
		file.writeToFile(std::string(text));
		ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;
		collectInputsFor(file, new_inputs);

		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
	}

	void CompilerFilesManagement::closeDocument(const fs::FilePath& path) {
		auto cache_twin = session->cachePath(path);
		if (!cache_twin.exists()) return;

		auto cache_file = fs::File(cache_twin);

		if (path.isRegularFile()) {
			std::vector<query::external::InputData> previous_inputs;
			previous_inputs.push_back(pst::SourcePositionLocked::getQueryInputNode());
			collectInputsFor(cache_file, previous_inputs);

			if_opt_some(findModuleForFile(cache_file), module_ref) {
				ModuleTreeModifier::removeMainSourceFile(module_ref);
				ModuleTreeModifier::setMainSourceFile(module_ref, fs::File(path));
			}

			std::vector<query::external::InputData> new_inputs;
			collectInputsFor(fs::File(path), new_inputs);

			// Only the virtual twin is deleted here; the file on disk is never touched.
			fs::FileManager::deleteFile(cache_file);

			query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
			return;
		}

		// The buffer never existed on disk, so there is nothing to swap back to.
		fs::FileManager::deleteFile(cache_file);
		reloadPackageOwning(path);
	}

	void CompilerFilesManagement::fileCreatedOrDeletedOnDisk(const fs::FilePath& path) {
		reloadPackageOwning(path);
	}

}
