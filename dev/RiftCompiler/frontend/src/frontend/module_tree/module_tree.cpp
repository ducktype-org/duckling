/**
 * @file module_tree.cpp
 * @author Mateusz Kołpa (matihopemine@gmail.com)
 */

#include "module_tree.hpp"

#include <utility>
#include <iostream>

bool compiler::frontend::ModuleTree::isEmpty() const {
	return m_source_files.empty() && m_submodules.empty() && m_other_files.empty();
}

void compiler::frontend::ModuleTree::buildModuleTree(
	const std::shared_ptr<ModuleTree>& module_root, std::shared_ptr<FsTree> tree_root
) {
	module_root->m_fs_tree = std::move(tree_root);
	for (const auto& file_iter: module_root->m_fs_tree->getFiles()) {
		auto        std_path  = file_iter.second.getStdPath();
		std::string stem      = std_path.stem();
		std::string extension = std_path.extension();

		if (extension == RIFT_SOURCE_FILE) {
			if (std_path.filename() == RIFT_MAIN_SOURCE_FILE)
				module_root->m_main_source_file = base::make_unique<fs::FilePath>(file_iter.second);
			else
				module_root->m_source_files.push_back(file_iter.second);
		} else if (extension == RIFT_MODULE_FILE) {
			auto submodule = std::shared_ptr<ModuleTree>(new ModuleTree());
			module_root->m_submodules.put(stem, submodule);
			submodule->m_main_source_file = base::make_unique<fs::FilePath>(file_iter.second);
			submodule->m_parent           = module_root;
		} else {
			if (!module_root->m_other_files.contains(extension))
				module_root->m_other_files.put(extension, std::vector<fs::FilePath>());
			module_root->m_other_files[extension].push_back(file_iter.second);
		}
	}

	for (const auto& dir_iter: module_root->m_fs_tree->getDirs()) {
		auto submodule = ModuleTree::create(dir_iter.second);
		if (!submodule->isEmpty()) module_root->m_submodules.put(dir_iter.first, submodule);
	}

	if (!module_root->isEmpty() && module_root->m_main_source_file == nullptr)
		throw std::logic_error(base::strConcat(
			"No main source file in: ", module_root->m_fs_tree->getRoot().absolutePath()
		));
}

base::Optional<const compiler::frontend::ModuleTree&>
	compiler::frontend::ModuleTree::getParentModule() const {
	if (m_parent == nullptr) return {};
	return *m_parent;
}

std::string compiler::frontend::ModuleTree::getName() const {
	if (m_fs_tree == nullptr) return getMainSourceFile().getStdPath().stem();
	return m_fs_tree->getRoot().name();
}

void compiler::frontend::ModuleTree::prettyPrint(u32 indentation) const {
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

const fs::FilePath& compiler::frontend::ModuleTree::getMainSourceFile() const {
	if (m_main_source_file == nullptr) throw std::logic_error("No main source file!");
	return *m_main_source_file;
}
