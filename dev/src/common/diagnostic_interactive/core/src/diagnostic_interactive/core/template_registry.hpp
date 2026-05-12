#pragma once
#include "diagnostic_arguments_forward.hpp"
#include "template_file.hpp"

#include <concurrent/base/collections/hash_map.hpp>

#include <base/collections/maps.hpp>
#include <base/pointers/box.hpp>

#include <mutex>

namespace dia_int {

	/**
	 * @brief The provider interface for loading diagnostic templates.
	 * There are different providers for test purposes.
	 */
	class TemplateRegistryProvider {
	public:
		virtual ~TemplateRegistryProvider()                                          = default;
		virtual base::Optional<std::string_view> loadTemplate(std::string_view path) = 0;
	};

	/**
	 * @brief This is the main provider that loads templates from filesystem.
	 * Optionally this can be loaded from the embedded resources.
	 * This mechanism is configured at the CMake level.
	 */
	class TemplateResistryMainProvider final: public TemplateRegistryProvider {
	public:
		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	class TemplateRegistryFilesystemProvider final: public TemplateRegistryProvider {
		base::HashMap<std::string, std::string> templates;
		std::string                             templates_root;

	public:
		TemplateRegistryFilesystemProvider(std::string templates_root):
			  templates_root(std::move(templates_root)) {}

		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	class TemplateRegistryTestProvider final: public TemplateRegistryProvider {
		base::HashMap<std::string, std::string> templates;

	public:
		TemplateRegistryTestProvider(base::HashMap<std::string, std::string> templates):
			  templates(std::move(templates)) {}

		base::Optional<std::string_view> loadTemplate(std::string_view path) override;
	};

	/**
	 * @brief Registry for loading and caching diagnostic templates.
	 * It's implemented as a singleton.
	 */
	class TemplateRegistrySingleton final {
	private:
		concurrent::ConHashMap<std::string, template_file::DiagnosticTemplate> cache;
		Box<TemplateRegistryProvider>                                          provider;

		static std::mutex                      instance_mutex;
		static MBox<TemplateRegistrySingleton> instance;

		TemplateRegistrySingleton(Box<TemplateRegistryProvider> provider):
			  provider(std::move(provider)) {}

		template<class T, class Deleter, class... Args>
		friend base::Box<T, Deleter> base::makeBox(Args&&... args
		);  // @TODO: #1364 deal with friend makeBox

	public:
		/**
		 * @brief Main function, loads a template by metadata.
		 */
		template_file::DiagnosticTemplate& loadTemplate(const dia_args::Metadata& metadata);

		static TemplateRegistrySingleton& getInstance();

		/**
		 * @brief Set the instance object based on the provider.
		 * When called multiple times, only the first call will have an effect.
		 */
		static void setInstance(Box<TemplateRegistryProvider>&& provider);

		/**
		 * @brief Force the registry to use a new provider.
		 * For testing purposes.
		 */
		static void setNewInstance(Box<TemplateRegistryProvider>&& provider);
	};

}
