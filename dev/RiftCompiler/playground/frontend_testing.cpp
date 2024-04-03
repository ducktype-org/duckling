#include <frontend/module_tree/module_tree.hpp>
#include <clap/clap.hpp>
#include <iostream>

int main(int argc, const char* argv[]) {
	auto clap
		= clap::Clap()
	          .addHelpFlag()
	          .add(clap::ParamBuilder::ofValue(clap::FileParser::make("Path"))
	                   .addShortName('p')
	                   .addShortDesc("Path to Rift source root")
					   .required()
	                   .build());

	clap::ParsingResult options;
	try {
		options = clap.parse(argc, argv);
	} catch (clap::exceptions::HelpException& e) {
		std::cout << clap::HelpMessageGenerator::generate(clap, e.parsing_result) << '\n';
		return 1;
	} catch (clap::exceptions::ClapException& e) {
		std::cout << e.what() << '\n';
		return 1;
	}


	auto path_to_compile = options.getValue<fs::FilePath>('p').value();

	std::cout << "path_to_compile: " << path_to_compile.strView() << "\n";


	using compiler::frontend::ModuleTree;

	std::shared_ptr<ModuleTree> module_tree = ModuleTree::create(path_to_compile);

	module_tree->prettyPrint();

// 	// Print main source file's content.
// 	if (module_tree->hasMainSourceFile())
// 		std::cout << module_tree->getMainSourceFile().getContent().view().stringView() << '\n';

// 	// Print content of source files.
// 	for (const fs::FilePath& file: module_tree->getSourceFiles())
// 		std::cout << file.getContent().view().stringView() << '\n';

// 	// Print names of other modules.
// 	//
// 	// getSubmodules is an iterator:
// 	// first  - name
// 	// second - std::shared_ptr<ModuleTree>
// 	for (const auto& submodule: module_tree->getSubmodules())
// 		std::cout << submodule.first << " == " << submodule.second->getName() << '\n';

}
