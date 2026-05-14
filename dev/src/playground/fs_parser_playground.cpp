#include <frontend/module_tree/functors.hpp>
#include <frontend/module_tree/module_tree.hpp>

#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>

#include <iostream>

int file_counter = 0;
int dir_counter  = 0;

// Recursively count files and directories in a ModuleTree
void countFiles(const compiler::frontend::ModuleTree& tree) {
	file_counter += tree.hasMainSourceFile() ? 1 : 0;
	for (const auto& [_, files]: tree.getOtherFiles()) file_counter += (int) files.size();
	for (const auto& submodule: tree.getSubmodules().illegalAccess()) {
		countFiles(*compiler::frontend::getModuleRef(submodule.illegalAccess().getID()));
		dir_counter++;
	}
}

int main(int argc, const char* argv[]) {
	auto clah
		= clah::Clah("fs_parser_playground")
	          .add(clah::ParamBuilder::ofValue(clah::FileParser::make("Path"))
	                   .addShortName('p')
	                   .addShortDesc("Path to start the search")
	                   .build())
	          .add(clah::ParamBuilder::ofFlag()
	                   .addLongName("noprint")
	                   .addShortDesc(
						   "If this flag is passed, then do not print the filesystem structure."
					   )
	                   .build())
	          .add(clah::ParamBuilder::ofValue(clah::StringParser::make("File regex"))
	                   .addShortName('f')
	                   .addLongName("fileregex")
	                   .addShortDesc("A file rejecting regex")
	                   .build())
	          .add(clah::ParamBuilder::ofValue(clah::StringParser::make("Directory regex"))
	                   .addShortName('d')
	                   .addLongName("dirregex")
	                   .addShortDesc("A directory rejecting regex")
	                   .build());

	clah::ParsingResult res;
	try {
		res = clah.parse(base::safeIntConv<usize>(argc), argv);
	} catch (clah::exceptions::HelpException& e) {
		std::cout << clah::HelpMessageGenerator::generate(clah, e.parsing_result) << '\n';
	}

	auto module_tree = compiler::frontend::ModuleTreeBuilder::createWithRandomPackageID(
		res.getValue<fs::File>('p').copyValueOr(fs::File(".")),
		std::regex(res.getValue<std::string>("fileregex").copyValueOr("\\.*")),
		std::regex(res.getValue<std::string>("dirregex").copyValueOr("\\..*"))
	);

	if (!res.isFlag("noprint")) std::cout << module_tree->prettyPrint();
	countFiles(*module_tree);
	std::cout << "Files: " << file_counter << ", directories: " << dir_counter
			  << ", sum: " << file_counter + dir_counter << '\n';
}
