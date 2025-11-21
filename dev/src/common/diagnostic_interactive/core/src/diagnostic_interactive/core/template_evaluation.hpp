#pragma once
#include "diagnostic_file.hpp"
#include "template_file.hpp"
#include "diagnostic_state.hpp"

#include "base/maps.hpp"
#include <base/box.hpp>

namespace dia_app {

	// Forward declarations
	class TemplateRegistry;
	class ConstructTextViewVisitor;
	class EvaluateTemplateFileVisitor;
	class EvaluateDiagnosticFileVisitor;

	class TemplateRegistryProvider {
	public:
		virtual ~TemplateRegistryProvider()                                          = default;
		virtual base::Optional<std::string_view> loadTemplate(std::string_view path) = 0;
	};

	class TemplateResistryEmbeddedProvider: public TemplateRegistryProvider {
	public:
		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	class TemplateRegistryFilesystemProvider: public TemplateRegistryProvider {
		base::HashMap<std::string, std::string> templates;
		std::string                             templates_root;

	public:
		TemplateRegistryFilesystemProvider(std::string templates_root):
			  templates_root(std::move(templates_root)) {}

		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	class TemplateRegistryTestProvider: public TemplateRegistryProvider {
		base::HashMap<std::string, std::string> templates;

	public:
		TemplateRegistryTestProvider(base::HashMap<std::string, std::string> templates):
			  templates(std::move(templates)) {}

		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	/**
	 * @brief Registry for loading and caching diagnostic templates.
	 */
	class TemplateRegistry {
	private:
		base::HashMap<std::string, template_file::DiagnosticTemplate> cache;
		Box<TemplateRegistryProvider>                                 provider;

		static MBox<TemplateRegistry> instance;

	public:
		TemplateRegistry(Box<TemplateRegistryProvider> provider): provider(std::move(provider)) {}

		/**
		 * @brief Load a template by metadata.
		 */
		template_file::DiagnosticTemplate& loadTemplate(const dia_file::Metadata& metadata);

		static TemplateRegistry& getInstance();

		static void setInstance(Box<TemplateRegistryProvider> provider);
	};

	state::Message evaluateMessage(
		const template_file::MessageTemplate&                message_template,
		const dia_file::Message&                             message,
		TemplateRegistry&                                    registry,
		const base::HashMap<std::string, dia_file::Message>* attached_messages = nullptr
	);

	state::Diagnostic evaluateDiagnostic(const dia_file::Thread& thread);

	std::string constructTextView(CRef<state::Component> component);
}
