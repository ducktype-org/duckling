#include "module_tree.hpp"

#include "queries.hpp"

#include <base/exceptions.hpp>
#include <base/stable_container.hpp>
#include <base/string_id.hpp>

#include <query_framework/query_impl.hpp>

#include <algorithm>
#include <regex>
#include <sstream>

namespace {
	/**
	 * @brief Map storting FileID of each parsed PST (by root element ID)
	 * @note: as of right now it is needed only for QueryPrimaryCodeScopeFor for acquiring
	 * the root scope via extendQueryModuleIDOfPST.
	 * @todo: Either delete root scopes and add to PST some kind of "module nodes" or put
	 * information from this map into PST nodes.
	 */
	inline static base::Map<pst::PstID, compiler::frontend::FileID> root_element_file_back_map;

	/**
	 * StableVector that stores all ModuleTree instances.
	 */
	base::StableVector<compiler::frontend::ModuleTree> modules;

	/**
	 * Checks if a file name is valid according to the reject regex.
	 * @param filename The file name to check.
	 * @param reject_file_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isFileNameValid(const std::string& filename, const std::regex& reject_file_regex) {
		std::smatch match;
		return !std::regex_match(filename, match, reject_file_regex);
	}

	/**
	 * Checks if a directory name is valid according to the reject regex.
	 * @param dirname The directory name to check.
	 * @param reject_directory_regex The regex to use for rejection.
	 * @return True if valid, false otherwise.
	 */
	bool isDirectoryNameValid(const std::string& dirname, const std::regex& reject_directory_regex) {
		std::smatch match;
		return !std::regex_match(dirname, match, reject_directory_regex);
	}
}

namespace compiler::frontend {
	Ref<ModuleTree> ModuleTreeBuilder::create(
		const fs::File& root, const std::regex& file_reject, const std::regex& dir_reject
	) {
		base::Box<ModuleTreeBuilder> builder = ModuleTreeBuilder::create();

		if (root.isDirectory())
			builder->buildFromDirectory(root, file_reject, dir_reject);
		else
			builder->buildFromSingleFile(root);

		return builder->finalize();
	}

	ModuleTree::ModuleTree() {}

	base::Optional<base::CRef<ModuleTree>> ModuleTree::getParentModule() const {
		if (m_parent.has_value()) return m_parent.value();
		return {};
	}

	bool ModuleTree::hasMainSourceFile() const { return m_main_source_file.has_value(); }

	base::CRef<SourceFile> ModuleTree::getMainSourceFile() const {
		return m_main_source_file.value();
	}

	const std::vector<base::Ref<SourceFile>>& ModuleTree::getSourceFiles() const {
		return m_source_files;
	}

	const base::HashMap<base::StrID, base::Ref<ModuleTree>>& ModuleTree::getSubmodules() const {
		return m_submodules;
	}

	const base::HashMap<base::StrID, std::vector<fs::File>>& ModuleTree::getOtherFiles() const {
		return m_other_files;
	}

	base::StrID ModuleTree::getName() const { return m_name; }

	std::string ModuleTree::prettyPrint(u32 indentation) const {
		std::stringstream output;

		std::string indent;
		for (u32 i = 0; i < indentation % 3; i++) indent += " ";
		for (u32 i = 0; i < indentation - (indentation % 3); i++)
			indent += (i % 3 == 0 ? "│" : " ");

		output << indent << getName().strView() << "/ [name: " << getName().strView() << "]\n";

		if (hasMainSourceFile())
			output << indent << "├> " << getMainSourceFile()->getFile().name() << '\n';
		else
			output << indent << "├> Missing main module file!\n";

		for (const auto& file_ref: getSourceFiles())
			output << indent << "├= " << file_ref->getFile().name() << '\n';

		for (const auto& [ext, files]: getOtherFiles())
			for (const auto& file: files) output << indent << "├─ " << file.name() << '\n';

		for (const auto& [name, submodule_ref]: getSubmodules())
			output << submodule_ref->prettyPrint(indentation + 3);

		return output.str();
	}

	void ModuleTreeBuilder::buildFromDirectory(
		const fs::File& directory, const std::regex& file_reject, const std::regex& dir_reject
	) {
		CORE_ASSERT(
			directory.isDirectory(),
			base::strConcat("Expected directory, got file: ", directory.getFilePath().string())
		);

		setName(base::StrID(directory.name().c_str()));

		// Process all files and directories in the current directory
		for (const auto& path: directory.listFilePaths()) {
			// Skip symlinks to avoid cycles
			if (path.isSymlink()) continue;

			fs::File file(path);

			if (file.isDirectory()) {
				// Handle subdirectory
				if (!isDirectoryNameValid(file.name(), dir_reject)) continue;

				auto submodule = ModuleTreeBuilder::create(file, file_reject, dir_reject);
				CORE_ASSERT(
					submodule->getName() == base::StrID(file.name().c_str()),
					"Submodule name does not match"
				);

				// Discards directories without main module file:
				// @TODO: decide if this behavior is desirable
				if (submodule->hasMainSourceFile()) addSubmodule(submodule);
			} else {
				// Handle regular file
				if (!isFileNameValid(file.name(), file_reject)) continue;
				handleNewFile(file);
			}
		}
	}

	void ModuleTreeBuilder::buildFromSingleFile(const fs::File& file) {
		std::string stem      = file.stem();
		std::string extension = file.extension();
		CORE_ASSERT(
			extension == LANG_MODULE_FILE,
			"Expected a module file, got: " + file.getFilePath().string()
		);
		setName(base::StrID(stem.c_str()));
		setMainSourceFile(file);
	}

	void ModuleTreeBuilder::handleNewFile(const fs::File& file) {
		CORE_ASSERT(
			file.isFile(),
			base::strConcat("Expected file, got directory: ", file.getFilePath().string())
		);
		std::string stem      = file.stem();
		std::string extension = file.extension();

		if (extension == LANG_SOURCE_FILE) {
			// Regular source file - store path for later
			addSourceFile(file);
		} else if (extension == LANG_MODULE_FILE) {
			// Module file
			base::StrID stem_id(stem.c_str());

			if (stem_id == m_name) {
				setMainSourceFile(file);
			} else {
				auto submodule = ModuleTreeBuilder::create(file);
				CORE_ASSERT(submodule->getName() == stem_id, "Submodule name does not match");
				addSubmodule(base::Ref<ModuleTree>(submodule));
			}
		} else {
			// Other file
			addOtherFile(file);
		}
	}

	/*********************
	 * ModuleTreeBuilder Implementation
	 *********************/

	ModuleTreeBuilder::ModuleTreeBuilder(): m_finalized(false) {}

	base::Box<ModuleTreeBuilder> ModuleTreeBuilder::create() {
		return base::makeBox<ModuleTreeBuilder>(ModuleTreeBuilder());
	}

	void ModuleTreeBuilder::addSourceFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_source_file_paths.push_back(file);
	}

	void ModuleTreeBuilder::setMainSourceFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(!m_main_source_file_path.has_value(), "Main source file already set");
		m_main_source_file_path = file;
	}

	void ModuleTreeBuilder::addSubmodule(base::Ref<ModuleTree> submodule) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(
			!m_submodules.contains(submodule->getName()),
			"Submodule with the same name already added"
		);
		m_submodules.put(submodule->getName(), submodule);
	}

	void ModuleTreeBuilder::addOtherFile(const fs::File& file) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!m_other_files.contains(ext_id)) m_other_files.put(ext_id, std::vector<fs::File>());
		m_other_files.at(ext_id).push_back(file);
	}

	void ModuleTreeBuilder::setName(base::StrID name) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		CORE_ASSERT(m_name.isBad(), "Module name is already set");
		m_name = name;
	}

	void ModuleTreeBuilder::setParent(base::Ref<ModuleTree> parent) {
		CORE_ASSERT(!m_finalized, "Builder already finalized");
		m_parent = parent;
	}

	bool ModuleTreeBuilder::isFinalized() const { return m_finalized; }

	base::Ref<ModuleTree> ModuleTreeBuilder::finalize() {
		CORE_ASSERT(!m_finalized, "Builder already finalized");

		m_finalized = true;

		// Create new ModuleTree instance
		modules.pushBack(ModuleTree());
		Ref<ModuleTree> module_ref = modules.last();
		ModuleID        mod_id(module_ref);

		// Set ID and name
		module_ref->m_name        = m_name;
		module_ref->m_other_files = std::move(m_other_files);

		// Create SourceFiles from stored paths
		if (m_main_source_file_path.has_value()) {
			module_ref->m_main_source_file
				= SourceFile::create(m_main_source_file_path.value(), mod_id);
		}

		for (const auto& file_path: m_source_file_paths) {
			auto source_file = SourceFile::create(file_path, mod_id);
			module_ref->m_source_files.push_back(source_file);
		}

		if (m_parent.has_value()) ModuleTreeModifier::setParent(module_ref, m_parent);

		for (const auto& [name, submodule]: m_submodules)
			ModuleTreeModifier::addSubmodule(module_ref, submodule);

		return module_ref;
	}

	/*********************
	 * ModuleTreeModifier Implementation
	 *********************/

	void ModuleTreeModifier::addSourceFile(base::Ref<ModuleTree> module, const fs::File& file) {
		module->m_source_files.push_back(SourceFile::create(file, ModuleID(module)));
	}

	void ModuleTreeModifier::removeSourceFile([[maybe_unused]] query::Context& ctx, FileID file_id) {
		Ref<SourceFile> file   = file_id.ref;
		Ref<ModuleTree> module = file->getModule().ref;

		auto& source_files = module->m_source_files;
		auto  it
			= std::ranges::find_if(source_files, [file_id](const base::Ref<SourceFile>& source_file) {
				  return source_file == file_id.ref;
			  });

		CORE_ASSERT(it != source_files.end(), "SourceFile not found in module");

		// Delete SourceFiles
		source_files.erase(it);
		file->erase();
	}

	void ModuleTreeModifier::setMainSourceFile(base::Ref<ModuleTree> module, const fs::File& file) {
		CORE_ASSERT(
			!module->m_main_source_file.has_value(),
			"Main source file is already set, remove it first"
		);
		module->m_main_source_file = SourceFile::create(file, ModuleID(module));
	}

	void ModuleTreeModifier::addSubmodule(
		base::Ref<ModuleTree> module, base::Ref<ModuleTree> submodule
	) {
		base::StrID name = submodule->getName();
		CORE_ASSERT(
			!module->m_submodules.contains(name),
			base::strConcat(
				"Submodule with name '",
				name.strView(),
				"' already exists in module ",
				module->getName().strView(),
				" call remove first!"
			)
		);


		module->m_submodules.put(name, submodule);

		CORE_ASSERT(
			!submodule->m_parent.has_value(),
			base::strConcat("Submodule ", name.strView(), " already has a parent")
		);
		submodule->m_parent = module;
	}

	void ModuleTreeModifier::addOtherFile(base::Ref<ModuleTree> module, const fs::File& file) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		if (!module->m_other_files.contains(ext_id))
			module->m_other_files.put(ext_id, std::vector<fs::File>());

		CORE_ASSERT(
			!std::ranges::any_of(
				module->m_other_files.at(ext_id),
				[&file](const fs::File& f) { return f.getFilePath() == file.getFilePath(); }
			),
			base::strConcat(
				"Other file with path '",
				file.getFilePath().string(),
				"' already exists in module ",
				module->getName().strView()
			)
		);

		module->m_other_files.at(ext_id).push_back(file);
		//@TODO: do we need to update the module here?
		// module->update();
	}

	void ModuleTreeModifier::removeMainSourceFile(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_main_source_file.has_value(),
			base::strConcat(
				"Module ", module->getName().strView(), " does not have a main source file"
			)
		);
		module->m_main_source_file = {};
	}

	void ModuleTreeModifier::removeOtherFile(base::Ref<ModuleTree> module, const fs::File& file) {
		std::string extension = file.extension();
		base::StrID ext_id(extension.c_str());

		CORE_ASSERT(
			module->m_other_files.contains(ext_id),
			base::strConcat(
				"Other file with extension '",
				ext_id.strView(),
				"' does not exist in module ",
				module->getName().strView()
			)
		);

		auto& files = module->m_other_files.at(ext_id);
		auto  it    = std::ranges::find_if(files, [&file](const fs::File& f) {
            return f.getFilePath() == file.getFilePath();
        });

		CORE_ASSERT(
			it != files.end(),
			base::strConcat(
				"Other file with path '",
				file.getFilePath().string(),
				"' does not exist in module ",
				module->getName().strView()
			)
		);

		files.erase(it);
		//@TODO: do we need to update the module here?
		// module->update();
	}

	void ModuleTreeModifier::setParent(
		base::Ref<ModuleTree> module, base::Optional<base::Ref<ModuleTree>> parent
	) {
		CORE_ASSERT(parent.has_value(), "Parent module must be specified");
		addSubmodule(parent.value(), module);
	}

	void ModuleTreeModifier::removeParent(base::Ref<ModuleTree> module) {
		CORE_ASSERT(
			module->m_parent.has_value(),
			base::strConcat("Module ", module->getName().strView(), " does not have a parent")
		);

		// remove this module from its parent's submodules
		auto  parent     = module->m_parent.value();
		auto& submodules = parent->m_submodules;
		auto  it         = std::ranges::find_if(submodules, [module](const auto& pair) {
            return pair.second == module;
        });
		CORE_ASSERT(
			it != submodules.end(),
			base::strConcat(
				"Submodule with name ",
				module->getName(),
				" does not exist in parent module ",
				parent->getName().strView()
			)
		);
		submodules.erase(it);

		module->m_parent = {};
	}

	void ModuleTreeModifier::removeModule(ModuleID module_id) {
		Ref<ModuleTree> module_ref = module_id.ref;
		auto            parent     = module_ref->m_parent;

		// Remove all source files associated with this module
		for (const auto& file: module_ref->m_source_files) file->erase();

		// Remove main source file if it exists
		if (module_ref->m_main_source_file.has_value())
			module_ref->m_main_source_file.value()->erase();

		// Update parent module if it exists
		if (parent.has_value()) {
			// Remove the submodule from the parent's submodules
			auto& submodules = parent.value()->m_submodules;
			auto  it         = std::ranges::find_if(submodules, [module_id](const auto& pair) {
                return pair.second == module_id.ref;
            });
			CORE_ASSERT(
				it != submodules.end(),
				base::strConcat(
					"Submodule with ID ",
					module_id.ref->getName(),
					" does not exist in parent module ",
					parent.value()->getName().strView()
				)
			);
			submodules.erase(it);
		}

		// Remove module from vector
	}

	void ModuleTreeModifier::fileModified(FileID id) {
		Ref<SourceFile> source_file = id.ref;
		source_file->update();
	}

	// ----------------------

	base::StrID moduleName(ModuleID module) { return module.ref->getName(); }

	std::string printModuleTree(ModuleID module) { return module.ref->prettyPrint(); }

	/*********************
	 * QueryParentModule *
	 *********************/
	struct IMPLEMENT_QUERY(QueryParentModule, base::Optional<ModuleID>) {
		static auto provide(Context&, QKey key) -> PResult {
			auto module_tree = key.ref;
			return module_tree->getParentModule().map([](auto parent) {
				return ModuleID(const_cast<ModuleTree*>(&*parent));  // hacking here
			});
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryParentModule);

	/***********************
	 * QueryMainSourceFile *
	 ***********************/
	struct IMPLEMENT_QUERY(QueryMainSourceFile, FileID) {
		static auto provide(Context&, QKey key) -> PResult {
			auto module_tree = key.ref;
			return { const_cast<SourceFile*>(&*module_tree->getMainSourceFile()) };  // hacking here
		}

		QUERY_AUTO_NO_CACHE
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryMainSourceFile);

	/********************
	 * QuerySourceFiles *
	 ********************/
	struct IMPLEMENT_QUERY(QuerySourceFiles, std::vector<FileID>) {
		static auto provide(Context&, QKey key) -> PResult {
			const auto& module_tree = key.ref;

			std::vector<FileID> out{};
			for (const auto& file: module_tree->getSourceFiles()) out.push_back(FileID(file));
			return out;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySourceFiles);

	/*******************
	 * QuerySubmodules *
	 *******************/
	struct IMPLEMENT_QUERY(QuerySubmodules, base::HashMap<base::StrID COMMA ModuleID>) {
		static auto provide(Context&, QKey key) -> PResult {
			const auto& module_tree = key.ref;

			PResult out{};
			for (const auto& [name, module]: module_tree->getSubmodules())
				out.put(name, ModuleID(module));
			return out;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QuerySubmodules);

	/****************
	 * QueryFilePST *
	 ****************/
	struct IMPLEMENT_QUERY(QueryFilePST, CRef<pst::PST<>>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			Ref<SourceFile> file = key.ref;
			auto            pst  = file->getPST();
			root_element_file_back_map.put(pst->getRootElement().unlock(ctx)->getID(), key);

			// @todo modify it, when making proper helios errors
			if (pst->getLogger()->bad()) {
				std::cerr << "PARSING ERRORS: \n";
				pst->getLogger()->dumpLog(true, std::cerr);
				std::cerr << "\n\n";
			}

			return pst;
		}

		// @note: unstable ref here is only possible, because
		// PResult is already a reference
		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryFilePST);

	ModuleID extendQueryModuleIDOfPST(
		[[maybe_unused]] query::Context& ctx, pst::AccessLocked<pst::LangElement> element
	) {
		// get top-level:
		while (element.unlock(ctx)->getParent()) element = element.unlock(ctx)->getParent().value();

		// this access depends of global state that might become a problem in incremental compilation:
		auto file_id = root_element_file_back_map[element.unlock(ctx)->getID()];
		return file_id.ref->getModule();
	}
}
