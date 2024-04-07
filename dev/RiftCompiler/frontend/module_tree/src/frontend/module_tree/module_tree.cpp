/**
 * @file module_tree.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "module_tree.hpp"
#include <pst_parser/parser.hpp>
#include <map>

#include <query_framework/query_impl.hpp>
#include "queries.hpp"

using fs::FsTree;
using std::regex;
using namespace compiler::frontend;

inline static std::map<ModuleId, std::shared_ptr<ModuleTree>> modules{};
inline static std::map<FileId, SourceFile> files{};
inline static std::map<fs::FilePath, ModuleId> modulePaths{};

FileId::FileId() {}

FileId FileId::nextID() {
	// @OPT: move to global variable
	static u64 nextId = 0;

	FileId out;
	out.id = nextId++;
	return out;
}

ModuleId::ModuleId() {}

ModuleId ModuleId::nextID() {
	// @OPT: move to global variable
	static u64 nextId = 0;

	ModuleId out;
	out.id = nextId++;
	return out;
}

SourceFile::SourceFile(fs::FilePath path): path(path), id(FileId::nextID()) {
	rift_file_name = path.stem();
	files.insert({id, this});
}

const pst::PST& SourceFile::getPST() {
	if (parse_tree) {
		return parse_tree.value();
	}
	else {
		parse_tree.emplace(pst::parse(path));
		return parse_tree.value();
	}
}


ModuleTree::ModuleTree(): id(ModuleId::nextID()) {
	modules.insert{id, this};
	modulePaths.insert(getMainSourceFile()->path, id);
};

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
			module_root->m_submodules.put(dir_iter.first, submodule);
	}
}

void ModuleTree::handleNewFile(
	const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& filepath
) {
	if (filepath.isDirectory()) throw base::LogicError("File is not a file, but a directory!");

	std::string stem      = filepath.stem();
	std::string extension = filepath.extension();

	// There are 3 types of files: source files, module file, others - each if-branch handles other
	// type.
	if (extension == RIFT_SOURCE_FILE) {
		// File contains regular source content.
		module_root->m_source_files.push_back(filepath);
	} else if (extension == RIFT_MODULE_FILE) {
		// File with a config of SOME module.
		if (stem == module_root->getName()) {
			// File with a config of CURRENT module.

			// An assert for @aw5421 <3
			if (module_root->m_main_source_file.has_value())
				throw base::LogicError(base::strConcat("Module already has a main source file."));

			module_root->m_main_source_file.emplace(filepath);
		} else {
			// Single-file module.
			auto submodule = std::shared_ptr<ModuleTree>(new ModuleTree());
			module_root->m_submodules.put(stem, submodule);
			submodule->m_main_source_file.emplace(filepath);
			submodule->m_parent = module_root;
		}
	} else {
		// File contains content not related to the module.
		if (!module_root->m_other_files.contains(extension))
			module_root->m_other_files.put(extension, std::vector<fs::FilePath>());
		module_root->m_other_files[extension].push_back(filepath);
	}
}

base::Optional<const ModuleTree&> ModuleTree::getParentModule() const {
	if (m_parent.expired()) return {};
	return *m_parent.lock();
}

std::string ModuleTree::getName() const {
	if (m_fs_tree == nullptr) return getMainSourceFile().rift_file_name;
	return m_fs_tree->getRoot().name();
}

std::string ModuleTree::prettyPrint(u32 indentation) const {
	std::stringstream output;

	std::string indent;
	for (u32 i = 0; i < indentation % 3; i++) indent += " ";
	for (u32 i = 0; i < indentation - (indentation % 3); i++) indent += (i % 3 == 0 ? "│" : " ");

	output << indent << getName() << "/\n";

	if (m_main_source_file.has_value()) {
		output << indent << "├> " << m_main_source_file.value().path.name() << '\n';
	}
	else {
		output << indent << "├> Missing main module file!\n";
	}
	
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
	if (m_main_source_file.empty()) throw base::LogicError("No main source file!");
	return m_main_source_file.value();
}

const std::vector<SourceFile>& ModuleTree::getSourceFiles() const { return m_source_files; }

const base::HashMap<std::string, std::shared_ptr<ModuleTree>>& ModuleTree::getSubmodules() const {
	return m_submodules;
}

const base::HashMap<std::string, std::vector<fs::FilePath>>& ModuleTree::getOtherFiles() const {
	return m_other_files;
}

ModuleId ModuleTree::getId() const { return id; }

/*******************
 * GetPatentModule *
 *******************/
struct ImplementationOf_GetPatentModuleQuery: query::QueryImplementation<GetPatentModuleQuery, ModuleId> {
	inline static std::map<QKey, query::AddACD<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		std::shared_ptr<ModuleTree> module_tree = modules.at(key);

		return module_tree->getParentModule()->getId();
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetPatentModuleQuery, "GetPatentModuleQuery");

/*********************
 * GetMainSourceFile *
 *********************/
struct ImplementationOf_GetMainSourceFileQuery: query::QueryImplementation<GetMainSourceFileQuery, FileId> {
	inline static std::map<QKey, query::AddACD<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		std::shared_ptr<ModuleTree> module_tree = modules.at(key);

		return module_tree->getMainSourceFile()->id;
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetMainSourceFileQuery, "GetMainSourceFileQuery");

/******************
 * GetSourceFiles *
 ******************/
struct ImplementationOf_GetSourceFilesQuery: query::QueryImplementation<GetSourceFilesQuery, std::vector<FileId>&> {
	inline static std::map<QKey, query::AddACD<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		std::shared_ptr<ModuleTree> module_tree = modules.at(key);
		
		std::vector<FileId>& out{};
		for (auto file : module_tree->getSourceFiles()) {
			out.push_back(file.id);
		}
		return out;
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetSourceFilesQuery, "GetSourceFilesQuery");

/*****************
 * GetSubmodules *
 *****************/
struct ImplementationOf_GetSubmodulesQuery: query::QueryImplementation<GetSubmodulesQuery, base::HashMap<std::string, std::shared_ptr<ModuleId>>&> {
	inline static std::map<QKey, query::AddACD<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		std::shared_ptr<ModuleTree> module_tree = modules.at(key);
		
		base::HashMap<std::string, std::shared_ptr<ModuleId>>& out{};
		for (const auto& [name, module] : module_tree->getSourceFiles()) {
			out.insert({name, module->getId()});
		}
		return out;
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetSubmodulesQuery, "GetSubmodulesQuery");


/**************
 * GetFilePST *
 **************/
struct ImplementationOf_GetFilePSTQuery: query::QueryImplementation<GetFilePSTQuery, pst::PST&> {
	inline static std::map<QKey, query::AddACD<QResult>> cache{};

	static auto provide(Context& context, QKey key) -> PResult {
		SourceFile file = files.at(key);
		return file.getPST();
	}
	static auto load(QKey key) -> LoadResult {
		if (cache.contains(key))
			return cache.at(key);
		else
			return {};
	}
	static auto store(QKey key, PResult res, query::ACD acd) -> QResult {
		cache.insert({ key, { res, acd } });
		return res;
	}
};

QUERY_IMPLEMENTATION_BOILERPLATE(ImplementationOf_GetFilePSTQuery, "GetFilePSTQuery");
