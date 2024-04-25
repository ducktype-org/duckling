#include <clap/clap.hpp>
#include <clap/exceptions.hpp>
#include <clap/param_builder.hpp>
#include <iostream>
#include <array>
#include <vector>

#include "export_keywords.hpp"


using clap::Clap;
using std::array;
using std::string;
using std::vector;

int main(int argc, char** argv) {
	auto par = Clap()
	               .setDefaultParser(clap::StringParser::make())
	               .add(clap::ParamBuilder::ofFlag()
	                        .addShortName('e')
	                        .addLongName("export_keywords")
	                        .addShortDesc("Export keywords")
	                        .addLongDesc("Export keywords from lexer as JSON")
	                        .build());
	auto res = par.parse(argc, (const char**) argv);
	if (res.isFlag("export_keywords")) {
		auto interface = lsp::LspInterface();
		std::cout << interface.getAllJson() << "\n";
	} else {
		std::cout << "No flag"
				  << "\n";
	}


	return 0;
}
