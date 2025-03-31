#include <frontend/module_tree/module_tree.hpp>
#include <init/init.hpp>

#include <iostream>

int main() {
	init::InitObject _;

	using compiler::frontend::ModuleTree;

	// First argument is some kind of a path to a module we want to parse.
	// It returns a std::shared_ptr.
	std::shared_ptr<ModuleTree> module_tree = ModuleTree::create("../tests/test_module");

	// Print main source file's content.
	if (module_tree->hasMainSourceFile())
		std::cout << module_tree->getMainSourceFile().path.getContent().view().stringView() << '\n';

	// Print content of source files.
	for (auto&& file: module_tree->getSourceFiles())
		std::cout << file.path.getContent().view().stringView() << '\n';

	// Print names of other modules.
	//
	// getSubmodules is an iterator:
	// first  - name
	// second - std::shared_ptr<ModuleTree>
	for (const auto& submodule: module_tree->getSubmodules())
		std::cout << submodule.first.strView() << " == " << submodule.second->getName().strView()
				  << '\n';
}
