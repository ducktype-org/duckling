#include <frontend/module_tree/module_tree.hpp>

#include <base/int_conv.hpp>

#include <clah/clah.hpp>

#include <iostream>

int file_counter = 0;
int dir_counter  = 0;

void countFiles(const fs::FsTree& tree) {
	file_counter += (int) tree.getFiles().size();
	for (const auto& subdir: tree.getDirs()) {
		countFiles(*subdir.second);
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

	auto fs_tree = fs::FsTree::create(
		res.getValue<fs::File>('p').copyValueOr(fs::File(".")),
		std::regex(res.getValue<std::string>("fileregex").copyValueOr("\\.*")),
		std::regex(res.getValue<std::string>("dirregex").copyValueOr("\\..*"))
	);
	if (!res.isFlag("noprint")) std::cout << fs_tree->prettyPrint();
	countFiles(*fs_tree);
	std::cout << "Files: " << file_counter << ", directories: " << dir_counter
			  << ", sum: " << file_counter + dir_counter << '\n';
}
