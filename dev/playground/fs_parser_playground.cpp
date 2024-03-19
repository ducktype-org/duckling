#include <iostream>
#include "frontend/module_tree/module_tree.hpp"

int main() {
	auto module_tree = compiler::frontend::ModuleTree::create("../../playground/example_module");
	module_tree->prettyPrint();
	auto fs_tree = fs::FsTree::create("/home/mateusz/code/rift/rift-dev/dev/RiftCompiler/frontend");
	std::cout << fs_tree->prettyPrint();
}
