#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <init/init.hpp>

#include <iostream>

int main() {
	init::InitObject _;

	using compiler::frontend::getFileRef;
	using compiler::frontend::ModuleTree;

	// First argument is some kind of a path to a module we want to parse.
	// It returns a std::shared_ptr.
	Ref<ModuleTree> module_tree = compiler::frontend::ModuleTreeBuilder::create(
		fs::File("../tests/test_module"), "test_package_id"
	);

	// Print main source file's content.
	if (module_tree->hasMainSourceFile())
		std::cout << getFileRef(module_tree->getMainSourceFile().illegalAccess().getID())
						 ->getFile()
						 .getContent()
						 .view()
						 .stringView()
				  << '\n';

	// Print content of source files.
	for (auto&& file_locked: module_tree->getSourceFiles()) {
		auto file = getFileRef(file_locked.illegalAccess().getID());
		std::cout << file->getFile().getContent().view().stringView() << '\n';
	}

	// Print names of other modules.
	//
	// getSubmodules is an iterator:
	// first  - name
	// second - module
	for (auto&& submodule: module_tree->getSubmodules())
		std::cout << submodule.first.strView() << " == "
				  << compiler::frontend::GetModuleID_Functor::get(
						 submodule.second.illegalAccess().getID()
					 )
						 ->getName()
						 .strView()
				  << '\n';
}
