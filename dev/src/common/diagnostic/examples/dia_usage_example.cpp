// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * Example of how to use diagnostic messages defined by this library.
 */
#include <diagnostic/core/template_registry.hpp>
#include <diagnostic/message.hpp>

/**
 * First we have to write a message template yaml file.
 * It can be located in the `dia_templates` subdirectory
 * or inline for tests (check TemplateResistryMainProvider below).
 *
 * Once we have the diagnostic message template we can write a class
 * that is a wrapper around it. The class defines a way to properly
 * use the template, for example what parameters to pass.
 */
class ExampleDocs final: public dia::MessageBase {
	/**
	 * Each MessageBase instance should point to the path of the message template.
	 */
	dia::Metadata getMetadata() const final {
		return {
			.template_type = "message", .type = "docs", .family = "example", .name = "example"
		};
	}

public:
	/**
	 * These parameters: "language_name" and "country_name" are defined by the yaml template.
	 * We have to make sure that we provide all arguments the template requires and in the correct
	 * type.
	 */
	ExampleDocs(std::string language_name, std::string country_name): MessageBase() {
		addArgument<dia::TextArgument>("language_name", std::move(language_name));
		addArgument<dia::TextArgument>("country_name", std::move(country_name));
	}
};

int main() {
	/**
	 * This is typically done once in a compiler.
	 * The `TemplateResistryMainProvider` uses the filesystem and the `dia_templates` subdirectory
	 * to search for templates. We could also use `TemplateRegistryTestProvider` where we pass as an
	 * argument a list of templates inline.
	 */
	dia::TemplateRegistrySingleton::setInstance(makeBox<dia::TemplateResistryMainProvider>());

	dia::Logger logger;

	/**
	 * We use a logger instance and pass it `Box<MessageBase>` - a filled tempate.
	 */
	logger.log(makeBox<ExampleDocs>("DuckLing", "Poland"));
	logger.terminalPrint(std::cout);

	return 0;
}
