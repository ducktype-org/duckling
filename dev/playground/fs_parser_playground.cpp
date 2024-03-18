#include "frontend/module_tree/module_tree.hpp"

int main() {
	auto module_tree = compiler::frontend::ModuleTree::create("../../playground/example_module");
	module_tree->prettyPrint();
}
