#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/module_tree_builder.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>

#include <iostream>

int main(int argc, const char* argv[]) {
	init::InitObject _;

	auto clah = clah::Clah("frontend_playground")
	                .add(clah::ParamBuilder::ofValue(clah::FileParser::make("Path"))
	                         .addShortName('p')
	                         .addShortDesc("Path to Duckling source root")
	                         .required()
	                         .build());

	clah::ParsingResult options;
	try {
		options = clah.parse(static_cast<usize>(argc), argv);
	} catch (clah::exceptions::HelpException& e) {
		std::cerr << clah::HelpMessageGenerator::generate(clah, e.parsing_result) << '\n';
		return 1;
	} catch (clah::exceptions::ClahException& e) {
		std::cerr << e.what() << '\n';
		return 1;
	}


	auto path_to_compile = options.getValue<fs::File>('p').value();

	std::cerr << "path_to_compile: " << path_to_compile.getFilePath().strView() << "\n";


	using compiler::frontend::ModuleTree;

	base::Ref<ModuleTree> module_tree
		= compiler::frontend::ModuleTreeBuilder::createWithRandomPackageID(path_to_compile);

	std::cerr << module_tree->prettyPrint();

	// 	// Print main source file's content.
	// 	if (module_tree->hasMainSourceFile())
	// 		std::cout << module_tree->getMainSourceFile().getContent().view().stringView() << '\n';

	// 	// Print content of source files.
	// 	for (const fs::File& file: module_tree->getSourceFiles())
	// 		std::cout << file.getContent().view().stringView() << '\n';

	// 	// Print names of other modules.
	// 	//
	// 	// getSubmodules is an iterator:
	// 	// first  - name
	// 	// second - std::shared_ptr<ModuleTree>
	// 	for (const auto& submodule: module_tree->getSubmodules())
	// 		std::cout << submodule.first << " == " << submodule.second->getName() << '\n';
}
