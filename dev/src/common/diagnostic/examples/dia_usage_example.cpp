#include <diagnostic/core/template_registry.hpp>
#include <diagnostic/message.hpp>

class ExampleDocs final: public dia::MessageBase {
	dia::Metadata getMetadata() const final {
		return {
			.template_type = "message", .type = "docs", .family = "example", .name = "example"
		};
	}

public:
	ExampleDocs(std::string language_name, std::string country_name): MessageBase() {
		addArgument<dia::TextArgument>("language_name", std::move(language_name));
		addArgument<dia::TextArgument>("country_name", std::move(country_name));
	}
};

int main() {
	// InitOb
	dia::TemplateRegistrySingleton::setInstance(makeBox<dia::TemplateResistryMainProvider>());

	dia::Logger logger;

	logger.log(makeBox<ExampleDocs>("DuckLing", "Poland"));
	logger.terminalPrint(std::cout);

	return 0;
}
