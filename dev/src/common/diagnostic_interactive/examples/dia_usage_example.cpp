#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/message.hpp>

class ExampleDocs final: public dia_int::MessageBase {
	dia_int::Metadata getMetadata() const final {
		return {
			.template_type = "message", .type = "docs", .family = "example", .name = "example"
		};
	}

public:
	ExampleDocs(std::string language_name, std::string country_name): MessageBase() {
		addArgument<dia_int::TextArgument>("language_name", std::move(language_name));
		addArgument<dia_int::TextArgument>("country_name", std::move(country_name));
	}
};

int main() {
	// InitOb
	dia_int::TemplateRegistrySingleton::setInstance(makeBox<dia_int::TemplateResistryMainProvider>()
	);

	dia_int::Logger logger;

	logger.log(makeBox<ExampleDocs>("DuckLing", "Poland"));
	logger.dumpLog(std::cout);

	return 0;
}
