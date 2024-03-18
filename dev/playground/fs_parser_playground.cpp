#include "frontend/fs_parser/fs_parser.hpp"
#include "frontend/module_tree/module_tree.hpp"
#include <iostream>

int counter = 0;

void print(const compiler::frontend::FsTree& fsp) {
	for (const auto& f: fsp.getFiles()) {
		counter++;
		std::cout << fsp.getRoot().absolutePath() << " ? " << f.first << '\n';
	}
	for (const auto& f: fsp.getDirs()) {
		counter++;
		std::cout << fsp.getRoot().absolutePath() << " ? " << f.first << '\n';
	}
	for (const auto& f: fsp.getDirs()) print(*f.second);
}

void printModules(const compiler::frontend::ModuleTree& module_tree) {
	std::cout << "Module: " << module_tree.getName() << '\n';

	for (const auto& f: module_tree.getSourceFiles()) std::cout << f.absolutePath() << '\n';

	for (const auto& submodule: module_tree.getSubmodules()) printModules(*submodule.second);
	for (const auto& e: module_tree.getOtherFiles()) {
		std::cout << e.first << ": \n";
		for (const auto& f: e.second) std::cout << '\t' << f.absolutePath() << '\n';
	}
}

int main() {
	auto fsp = compiler::frontend::FsTree::create("../../playground/example_module");
	print(*fsp);
	std::cout << "All files and dirs: " << counter << '\n';

	auto module_tree = compiler::frontend::ModuleTree::create("../../playground/example_module");
	printModules(*module_tree);
	// TODO: Add pretty print of module_tree.
}
