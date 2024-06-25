/**
 * @file module_tree.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "module_tree.hpp"

#include <pst_parser/parser.hpp>
#include <base/maps.hpp>
#include <query_framework/query_impl.hpp>

#include "queries.hpp"

using fs::FsTree;
using std::regex;
using namespace compiler::frontend;

inline static base::HashMap<ModuleId, std::shared_ptr<ModuleTree>> modules{};
inline static base::HashMap<FileId, SourceFile&>                   files{};
inline static base::HashMap<fs::FilePath, ModuleId>                modulePaths{};

FileId FileId::nextID() {
	// @OPT: move to global variable
	static u64 nextId = 0;

	FileId out{};
	out.id = nextId++;
	return out;
}

SourceFile::SourceFile(fs::FilePath path): path(std::move(path)), id(FileId::nextID()) {
	rift_file_name = base::StrId(this->path.stem().c_str());
}

const pst::PST<>& SourceFile::getPST() {
	if (parse_tree) {
		return parse_tree.value();
	} else {
		parse_tree.emplace(pst::PST(path));
		return parse_tree.value();
	}
}

ModuleTree::ModuleTree(): id(ModuleId::next()){};

std::shared_ptr<ModuleTree> ModuleTree::create(std::shared_ptr<fs::FsTree> root) {
	auto ptr = std::shared_ptr<ModuleTree>(new ModuleTree());

	buildModuleTree(ptr, std::move(root));

	modules.put(ptr->getId(), ptr);
	if (ptr->hasMainSourceFile()) modulePaths.put(ptr->getMainSourceFile().path, ptr->getId());

	// at this point references inside module tree are stable, so we can fill "files" map:
	if (ptr->hasMainSourceFile()) {
		auto& main = ptr->m_main_source_file.value();
		files.put(main.id, main);
	}
	for (auto& file: ptr->m_source_files) files.put(file.id, file);

	return ptr;
}

bool ModuleTree::hasMainSourceFile() const { return !m_main_source_file.empty(); }

void ModuleTree::buildModuleTree(
	const std::shared_ptr<ModuleTree>& module_root, std::shared_ptr<FsTree> tree_root
) {
	module_root->m_fs_tree = std::move(tree_root);

	// Process regular files.
	for (const auto& file_iter: module_root->m_fs_tree->getFiles())
		handleNewFile(module_root, file_iter.second);

	// Add directory submodules.
	for (const auto& dir_iter: module_root->m_fs_tree->getDirs()) {
		auto submodule = ModuleTree::create(dir_iter.second);

		// Discards directories without main module file:
		// @TODO: decide if this behavior is desirable
		if (submodule->hasMainSourceFile())
			module_root->m_submodules.put(base::StrId(dir_iter.first.c_str()), submodule);
	}
}

void ModuleTree::handleNewFile(
	const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& filepath
) {
	if (filepath.isDirectory()) throw base::LogicError("File is not a file, but a directory!");

	std::string stem      = filepath.stem();
	std::string extension = filepath.extension();

	auto stem_id = base::StrId(stem.c_str());
	auto extension_id = base::StrId(extension.c_str());

	// There are 3 types of files: source files, module file, others - each if-branch handles other
	// type.
	if (extension == RIFT_SOURCE_FILE) {
		// File contains regular source content.
		module_root->m_source_files.push_back(filepath);
	} else if (extension == RIFT_MODULE_FILE) {
		// File with a config of SOME module.
		if (stem_id == module_root->getName()) {
			// File with a config of CURRENT module.

			// An assert for @aw5421 <3
			if (module_root->m_main_source_file.has_value())
				throw base::LogicError(base::strConcat("Module already has a main source file."));

			module_root->m_main_source_file.emplace(filepath);
		} else {
			// Single-file module.
			auto submodule = std::shared_ptr<ModuleTree>(new ModuleTree());
			module_root->m_submodules.put(stem_id, submodule);
			submodule->m_main_source_file.emplace(filepath);
			submodule->m_parent = module_root;
		}
	} else {
		// File contains content not related to the module.
		if (!module_root->m_other_files.contains(extension_id))
			module_root->m_other_files.put(extension_id, std::vector<fs::FilePath>());
		module_root->m_other_files[extension_id].push_back(filepath);
	}
}

base::Optional<const ModuleTree&> ModuleTree::getParentModule() const {
	if (m_parent.expired()) return {};
	return *m_parent.lock();
}

base::StrId ModuleTree::getName() const {
	// @OPT: store this value as a module tree field
	if (m_fs_tree == nullptr) return getMainSourceFile().rift_file_name;
	return base::StrId(m_fs_tree->getRoot().name().c_str());
}

std::string ModuleTree::prettyPrint(u32 indentation) const {
	std::stringstream output;

	std::string indent;
	for (u32 i = 0; i < indentation % 3; i++) indent += " ";
	for (u32 i = 0; i < indentation - (indentation % 3); i++) indent += (i % 3 == 0 ? "│" : " ");

	output << indent << getName().strView() << "/\n";

	if (m_main_source_file.has_value())
		output << indent << "├> " << m_main_source_file.value().path.name() << '\n';
	else
		output << indent << "├> Missing main module file!\n";

	for (const auto& file_iter: getSourceFiles())
		output << indent << "├= " << file_iter.path.name() << '\n';

	for (const auto& file_iter: getOtherFiles())
		for (const auto& file_name: file_iter.second)
			output << indent << "├─ " << file_name.name() << '\n';

	for (const auto& submodule: getSubmodules())
		output << submodule.second->prettyPrint(indentation + 3);

	return output.str();
}

const SourceFile& ModuleTree::getMainSourceFile() const {
	if (m_main_source_file.empty())
		throw base::LogicError(base::strConcat("No main source file! in module: ", getName()));

	return m_main_source_file.value();
}

const std::vector<SourceFile>& ModuleTree::getSourceFiles() const { return m_source_files; }

const base::HashMap<base::StrId, std::shared_ptr<ModuleTree>>& ModuleTree::getSubmodules() const {
	return m_submodules;
}

const base::HashMap<base::StrId, std::vector<fs::FilePath>>& ModuleTree::getOtherFiles() const {
	return m_other_files;
}

ModuleId ModuleTree::getId() const { return id; }

/*********************
 * QueryParentModule *
 *********************/
struct IMPLEMENT_QUERY(QueryParentModule, ModuleId) {
	static auto provide(Context&, QKey key) -> PResult {
		std::shared_ptr<ModuleTree> module_tree = modules.at(key);
		return module_tree->getParentModule().value().getId();
	}

	static auto load(QKey) -> LoadResult { return {}; }

	static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(QueryParentModule);

/***********************
 * QueryMainSourceFile *
 ***********************/
struct IMPLEMENT_QUERY(QueryMainSourceFile, FileId) {
	static auto provide(Context&, QKey key) -> PResult {
		auto module_tree = modules.at(key);
		return module_tree->getMainSourceFile().id;
	}

	static auto load(QKey) -> LoadResult { return {}; }

	static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
};

QUERY_IMPLEMENTATION_BOILERPLATE(QueryMainSourceFile);

/********************
 * QuerySourceFiles *
 ********************/
struct IMPLEMENT_QUERY(QuerySourceFiles, std::vector<FileId>) {
	inline static base::HashMap<QKey, base::unique_ptr<PResWithACD>> cache{};

	static auto provide(Context&, QKey key) -> PResult {
		auto module_tree = modules.at(key);

		std::vector<FileId> out{};
		for (const auto& file: module_tree->getSourceFiles()) out.push_back(file.id);
		return out;
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) {
			auto entry = cache.at(key).borrow();
			return QResWithACD{ entry->data, entry->acd };
		} else {
			return {};
		}
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.put(key, base::make_unique<PResWithACD>(PResWithACD{ std::move(res), acd }));
		return cache.at(key)->data;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(QuerySourceFiles);

/*******************
 * QuerySubmodules *
 *******************/
struct IMPLEMENT_QUERY(QuerySubmodules, base::HashMap<base::StrId COMMA ModuleId>) {
	inline static base::HashMap<QKey, base::unique_ptr<PResWithACD>> cache{};

	static auto provide(Context&, QKey key) -> PResult {
		auto module_tree = modules.at(key);

		PResult out{};
		for (const auto& [name, module]: module_tree->getSubmodules())
			out.put(name, module->getId());
		return out;
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key)) {
			auto entry = cache.at(key).borrow();
			return QResWithACD{ entry->data, entry->acd };
		} else {
			return {};
		}
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.put(key, base::make_unique<PResWithACD>(PResWithACD{ std::move(res), acd }));
		return cache.at(key)->data;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(QuerySubmodules);

/****************
 * QueryFilePST *
 ****************/
struct IMPLEMENT_QUERY(QueryFilePST, const pst::PST<>&) {
	inline static base::HashMap<QKey, query::CacheEntry<QResult>> cache{};

	static auto provide(Context&, QKey key) -> PResult {
		auto& file = files.at(key);
		return file.getPST();
	}

	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}

	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.put(key, { res, acd });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(QueryFilePST);
