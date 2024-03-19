/**
 * @file module_tree.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "module_tree.hpp"

#include <utility>
#include <iostream>

using fs::FsTree;
using namespace compiler::frontend;

bool ModuleTree::isEmpty() const { return m_main_source_file == nullptr; }

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
		if (!submodule->isEmpty()) module_root->m_submodules.put(dir_iter.first, submodule);
	}

	// Verify if the module is valid.
	if (!module_root->isEmpty() && module_root->m_main_source_file == nullptr)
		throw std::logic_error(base::strConcat(
			"No main source file in: ", module_root->m_fs_tree->getRoot().absolutePath()
		));
}

void ModuleTree::handleNewFile(
	const std::shared_ptr<ModuleTree>& module_root, const fs::FilePath& file
) {
	const auto& std_path  = file.getStdPath();
	std::string stem      = std_path.stem();
	std::string extension = std_path.extension();

	if (extension == RIFT_SOURCE_FILE) {
		if (std_path.filename() == RIFT_MAIN_SOURCE_FILE)
			module_root->m_main_source_file = base::make_unique<fs::FilePath>(file);
		else
			module_root->m_source_files.push_back(file);
	} else if (extension == RIFT_MODULE_FILE) {
		auto submodule = std::shared_ptr<ModuleTree>(new ModuleTree());
		module_root->m_submodules.put(stem, submodule);
		submodule->m_main_source_file = base::make_unique<fs::FilePath>(file);
		submodule->m_parent           = module_root;
	} else {
		if (!module_root->m_other_files.contains(extension))
			module_root->m_other_files.put(extension, std::vector<fs::FilePath>());
		module_root->m_other_files[extension].push_back(file);
	}
}

base::Optional<const ModuleTree&> ModuleTree::getParentModule() const {
	if (m_parent == nullptr) return {};
	return *m_parent;
}

std::string ModuleTree::getName() const {
	if (m_fs_tree == nullptr) return getMainSourceFile().getStdPath().stem();
	return m_fs_tree->getRoot().name();
}

void ModuleTree::prettyPrint(u32 indentation) const {
	std::string indent;
	for (u32 i = 0; i < indentation; i++) indent += (i % 3 == 0 ? "│" : " ");

	std::cout << indent << getName() << "/\n";

	std::cout << indent << "├> " << m_main_source_file->name() << '\n';
	for (const auto& file_iter: getSourceFiles())
		std::cout << indent << "├= " << file_iter.name() << '\n';

	for (const auto& file_iter: getOtherFiles())
		for (const auto& file_name: file_iter.second)
			std::cout << indent << "├─ " << file_name.name() << '\n';

	for (const auto& submodule: getSubmodules()) submodule.second->prettyPrint(indentation + 3);
}

const fs::FilePath& ModuleTree::getMainSourceFile() const {
	if (m_main_source_file == nullptr) throw std::logic_error("No main source file!");
	return *m_main_source_file;
}
