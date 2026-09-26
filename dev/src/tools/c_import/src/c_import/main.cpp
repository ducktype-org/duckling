#include "options.hpp"
#include "run.hpp"

#include <clah/clah.hpp>

#include <array>
#include <string>
#include <vector>

namespace {

	auto ownParameters() {
		return std::array{
			clah::ParamBuilder::ofValue(
				clah::StringListParser::make("headers", clah::StringParser::make())
			)
				.addLongName("header")
				.addShortDesc("C headers to translate, comma separated.")
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
				.addLongName("package-name")
				.addShortDesc("Name of the generated package.")
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("dir"))
				.addLongName("out-dir")
				.addShortDesc("Directory to write the package to. Defaults to the package name.")
				.optional()
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("path"))
				.addLongName("library")
				.addShortDesc("Object or archive to link against. Should be an absolute path.")
				.optional()
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("args"))
				.addLongName("links-raw")
				.addShortDesc("Linker arguments to use verbatim instead of --library.")
				.optional()
				.build(),
			clah::ParamBuilder::ofValue(
				clah::StringListParser::make("paths", clah::StringParser::make())
			)
				.addLongName("dvm-shared-lib")
				.addShortDesc("Shared objects the DVM loads at runtime, comma separated.")
				.optional()
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("version"))
				.addLongName("version")
				.addShortDesc("Version written to the manifest. Defaults to 1.0.0.")
				.optional()
				.build(),
			clah::ParamBuilder::ofValue(clah::StringParser::make("std"))
				.addLongName("std")
				.addShortDesc("C standard passed to clang. Defaults to c17.")
				.optional()
				.build(),
			clah::ParamBuilder::ofFlag()
				.addLongName("split")
				.addShortDesc("Emit one module per translated header instead of a single one.")
				.build(),
			clah::ParamBuilder::ofFlag()
				.addLongName("force")
				.addShortDesc("Overwrite the output directory when it is not empty.")
				.build(),
			clah::ParamBuilder::ofFlag()
				.addLongName("ignore-parse-errors")
				.addShortDesc("Generate bindings even when clang reports errors.")
				.build(),
		};
	}

	c_import::Options optionsFrom(const clah::ParsingResult& parsed) {
		c_import::Options options;

		options.headers      = parsed.getValue<std::vector<std::string>>("header").copyValueOr({});
		options.package_name = parsed.getValue<std::string>("package-name").copyValueOr("");
		options.out_dir      = parsed.getValue<std::string>("out-dir").copyValueOr("");
		options.links        = parsed.getValue<std::string>("links-raw")
		                    .copyValueOr(parsed.getValue<std::string>("library").copyValueOr(""));
		options.dvm_shared_libs
			= parsed.getValue<std::vector<std::string>>("dvm-shared-lib").copyValueOr({});
		options.version  = parsed.getValue<std::string>("version").copyValueOr("1.0.0");
		options.std_flag = "-std=" + parsed.getValue<std::string>("std").copyValueOr("c17");
		options.split    = parsed.isFlag("split");
		options.force    = parsed.isFlag("force");
		options.ignore_parse_errors = parsed.isFlag("ignore-parse-errors");

		return options;
	}

}

int main(int argc, const char* const* argv) {
	auto split = c_import::splitArguments(argc, argv);

	std::vector<const char*> own;
	own.reserve(split.own.size());
	for (const auto& argument: split.own) own.push_back(argument.c_str());

	auto clah = clah::Clah("duck_c_import", "Generates a Duckling package from C headers")
	                .add(ownParameters())
	                .setHandler([&split](const clah::ParsingResult& parsed) -> int {
						auto options       = optionsFrom(parsed);
						options.clang_args = split.clang;
						return c_import::run(options);
					});

	return clah.execute(own.size(), own.data());
}
