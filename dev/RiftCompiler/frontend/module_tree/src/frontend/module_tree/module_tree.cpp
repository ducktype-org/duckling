/**
 * @file module_tree.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "module_tree.hpp"

using fs::FsTree;
using std::regex;
using namespace compiler::frontend;

regex ModuleTree::default_reject_file_regex      = regex(R"((\$.*|\..*))");
regex ModuleTree::default_reject_directory_regex = regex(R"((\$.*|\..*))");

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
		if (submodule->hasMainSourceFile())
			module_root->m_submodules.put(dir_iter.first, submodule);
	}
}

void ModuleTree::handleNewFile(
	const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& file
) {
	if (file.isDirectory()) throw std::logic_error("File is not a file, but a directory!");

	std::string stem      = file.stem();
	std::string extension = file.extension();

	if (extension == RIFT_SOURCE_FILE) {
		if (file.name() == RIFT_MAIN_SOURCE_FILE)
			module_root->m_main_source_file.emplace(file);
		else
			module_root->m_source_files.push_back(file);
	} else if (extension == RIFT_MODULE_FILE) {
		auto submodule = std::shared_ptr<ModuleTree>(new ModuleTree());
		module_root->m_submodules.put(stem, submodule);
		submodule->m_main_source_file.emplace(file);
		submodule->m_parent = module_root;
	} else {
		if (!module_root->m_other_files.contains(extension))
			module_root->m_other_files.put(extension, std::vector<fs::FilePath>());
		module_root->m_other_files[extension].push_back(file);
	}
}

base::Optional<const ModuleTree&> ModuleTree::getParentModule() const {
	if (m_parent.expired()) return {};
	return *m_parent.lock();
}

std::string ModuleTree::getName() const {
	if (m_fs_tree == nullptr) return getMainSourceFile().stem();
	return m_fs_tree->getRoot().name();
}

std::string ModuleTree::prettyPrint(u32 indentation) const {
	std::stringstream output;

	std::string indent;
	for (u32 i = 0; i < indentation % 3; i++) indent += " ";
	for (u32 i = 0; i < indentation - (indentation % 3); i++) indent += (i % 3 == 0 ? "│" : " ");

	output << indent << getName() << "/\n";

	output << indent << "├> " << m_main_source_file->name() << '\n';
	for (const auto& file_iter: getSourceFiles())
		output << indent << "├= " << file_iter.name() << '\n';

	for (const auto& file_iter: getOtherFiles())
		for (const auto& file_name: file_iter.second)
			output << indent << "├─ " << file_name.name() << '\n';

	for (const auto& submodule: getSubmodules())
		output << submodule.second->prettyPrint(indentation + 3);

	return output.str();
}

const fs::FilePath& ModuleTree::getMainSourceFile() const {
	if (m_main_source_file.empty()) throw std::logic_error("No main source file!");
	return *m_main_source_file;
}

const std::vector<fs::FilePath>& ModuleTree::getSourceFiles() const { return m_source_files; }

const base::HashMap<std::string, std::shared_ptr<ModuleTree>>& ModuleTree::getSubmodules() const {
	return m_submodules;
}

const base::HashMap<std::string, std::vector<fs::FilePath>>& ModuleTree::getOtherFiles() const {
	return m_other_files;
}
