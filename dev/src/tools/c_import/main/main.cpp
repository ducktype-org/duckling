// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file main.cpp
 * @brief `duckbind`: generates Duckling packages binding C libraries from their headers.
 */
#include <c_import/commands.hpp>

#include <base/misc/int_conv.hpp>

#include <clah/clah.hpp>
#include <init/init.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

	clah::Parameter listOption(const char* name, const char* description) {
		return clah::ParamBuilder::ofValue(
				   clah::StringListParser::make(name, clah::StringParser::make())
		)
		    .addLongName(name)
		    .addShortDesc(description)
		    .build();
	}

	clah::Parameter duckOption() {
		return clah::ParamBuilder::ofValue(clah::StringParser::make("path"))
		    .addLongName("duck")
		    .addShortDesc("The `duck` that checks the package (default: `duck` on PATH).")
		    .build();
	}

	clah::Parameter noVerifyFlag() {
		return clah::ParamBuilder::ofFlag()
		    .addLongName("no-verify")
		    .addShortDesc(
				"Do not build and run a program checking the package's layouts against C's."
			)
		    .build();
	}

	std::vector<std::string> list(const clah::ParsingResult& options, const char* name) {
		return options.getValue<std::vector<std::string>>(name).copyValueOr({});
	}

	std::optional<std::string> optionalString(const clah::ParsingResult& options, const char* name) {
		auto value = options.getValue<std::string>(name);
		return value ? std::optional<std::string>{ value.value() } : std::nullopt;
	}

	std::string duck(const clah::ParsingResult& options) {
		return optionalString(options, "duck").value_or("duck");
	}

	/// The optional package directory of `regen` and `check`.
	std::string packagePath(const clah::ParsingResult& options) {
		return options.getExtraParameterCount() > 0 ? options.getExtra<std::string>(0).value()
		                                            : ".";
	}

	clah::Clah newCommand() {
		return clah::Clah(
				   "new",
				   "Creates a package binding C headers. Lists are comma-separated; give a value "
				   "starting with `-` after `=` (`--cflag=-DFOO`)."
		)
		    .addPositional(clah::StringParser::make("path"), "Directory of the new package.")
		    .add(listOption(
				"header",
				"Headers to translate: a path, or a name found on the include path (`SDL3/SDL.h`)."
			))
		    .add(listOption("pkg-config", "pkg-config packages giving compiler flags and libraries.")
		    )
		    .add(listOption("cflag", "Extra compiler flags, such as `-I<dir>` and `-D<name>`."))
		    .add(listOption(
				"library", "`-l<name>`, `-L<dir>`, or a path to a `lib<name>.a`, `.so` or `.dylib`."
			))
		    .add(listOption("include", "Name globs a declaration has to match (default: all)."))
		    .add(listOption("exclude", "Name globs of declarations to leave out."))
		    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
		             .addLongName("name")
		             .addShortDesc("Package name (default: the directory's name).")
		             .build())
		    .add(noVerifyFlag())
		    .add(duckOption())
		    .setHandler([](const clah::ParsingResult& options) -> int {
				c_import::NewOptions new_options;
				new_options.at                = options.getPositional<std::string>(0);
				new_options.name              = optionalString(options, "name");
				new_options.recipe.headers    = list(options, "header");
				new_options.recipe.pkg_config = list(options, "pkg-config");
				new_options.recipe.cflags     = list(options, "cflag");
				new_options.recipe.libraries  = list(options, "library");
				new_options.recipe.include    = list(options, "include");
				new_options.recipe.exclude    = list(options, "exclude");
				new_options.verify            = !options.isFlag("no-verify");
				new_options.duck              = duck(options);
				return c_import::newPackage(std::move(new_options));
			});
	}

	clah::Clah regenCommand() {
		return clah::Clah("regen", "Regenerates a package from its `duckbind.yaml`.")
		    .setDefaultValueParser(clah::StringParser::make("path"))
		    .add(noVerifyFlag())
		    .add(duckOption())
		    .setHandler([](const clah::ParsingResult& options) -> int {
				return c_import::regenPackage(
					packagePath(options), !options.isFlag("no-verify"), duck(options)
				);
			});
	}

	clah::Clah checkCommand() {
		return clah::Clah(
				   "check",
				   "Fails when a package differs from what its `duckbind.yaml` generates here."
		)
		    .setDefaultValueParser(clah::StringParser::make("path"))
		    .setHandler([](const clah::ParsingResult& options) -> int {
				return c_import::checkPackage(packagePath(options));
			});
	}

	clah::Clah bindingsCommand() {
		return clah::Clah(
				   "bindings",
				   "Writes only the binding modules of a `duckbind.yaml` into a directory, for "
				   "build scripts: no manifest and no check."
		)
		    .addPositional(clah::StringParser::make("recipe"), "The `duckbind.yaml` to read.")
		    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("dir"))
		             .addLongName("out")
		             .addShortDesc("Directory to write the modules into.")
		             .required()
		             .build())
		    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("name"))
		             .addLongName("name")
		             .addShortDesc("Package name (default: the recipe's directory's name).")
		             .build())
		    .setHandler([](const clah::ParsingResult& options) -> int {
				return c_import::writeBindings(
					options.getPositional<std::string>(0),
					options.getValue<std::string>("out").value(),
					optionalString(options, "name")
				);
			});
	}

}

int main(int argc, const char* argv[]) {
	init::InitObject _;
	auto             cli = clah::Clah(
                   "duckbind", "Generates Duckling packages binding C libraries from their headers."
    )
	               .addSubcommand(newCommand())
	               .addSubcommand(regenCommand())
	               .addSubcommand(checkCommand())
	               .addSubcommand(bindingsCommand());
	try {
		return cli.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const std::exception& error) {
		std::cerr << "error: " << error.what() << "\n";
		return 1;
	}
}
