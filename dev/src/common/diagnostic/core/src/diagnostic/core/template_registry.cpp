// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "template_registry.hpp"

#include "exceptions.hpp"

#include <yaml-cpp/yaml.h>

#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/core/diagnostic_arguments.hpp>
#include <diagnostic/core/yaml_buffer.hpp>
#include <diagnostic/dia_templates/templates.hpp>
#include <filesystem/file.hpp>

#include <mutex>

namespace dia {

	// --------------------------------------------------------------------------------
	// Helper Functions
	// --------------------------------------------------------------------------------

	static bool checkMetadataMatch(
		const template_file::DiagnosticTemplate& tmpl, const dia_args::Metadata& metadata
	) {
		auto& template_metadata = tmpl.getMetadata();

		if (template_metadata.type != metadata.type) return false;
		if (template_metadata.family != metadata.family) return false;
		if (template_metadata.name != metadata.name) return false;

		variant_match(tmpl.content) {
			variant_case_novalue(template_file::MessageTemplate) {
				return metadata.template_type == "message";
			}
			variant_case_novalue(template_file::ComponentTemplate) {
				return metadata.template_type == "component";
			}
			variant_case_novalue(template_file::PointerMessageTemplate) {
				return metadata.template_type == "pointer_message";
			}
		}
		return false;
	}

	// --------------------------------------------------------------------------------
	// Template Registry Providers
	// --------------------------------------------------------------------------------

	base::Optional<std::string_view> TemplateResistryMainProvider::loadTemplate(std::string_view path
	) {
		auto result = dia::templates::loadTemplateFromPath(path);
		if (result.has_value()) return result.value();
		return {};
	}

	base::Optional<std::string_view> TemplateRegistryFilesystemProvider::loadTemplate(
		std::string_view path
	) {
		std::string path_str(path);
		auto        result = templates.atMaybe(path_str);
		if (result.has_value()) return std::string_view(*result.value());

		auto content_opt = fs::File(templates_root + path_str).getContentSafe();
		if (content_opt.has_value()) {
			std::string content_str(content_opt.value().view().stringView());
			templates.put(path_str, std::move(content_str));
			return std::string_view(*templates.atMaybe(path_str).value());
		}
		return {};
	}

	base::Optional<std::string_view> TemplateRegistryTestProvider::loadTemplate(std::string_view path
	) {
		return templates.atMaybe(std::string(path)).map([](Ref<std::string> str) {
			return std::string_view{ *str };
		});
	}

	// --------------------------------------------------------------------------------
	// TemplateRegistry Implementation
	// --------------------------------------------------------------------------------

	MBox<TemplateRegistrySingleton> TemplateRegistrySingleton::instance;
	std::mutex                      TemplateRegistrySingleton::instance_mutex;

	TemplateRegistrySingleton& TemplateRegistrySingleton::getInstance() {
		std::scoped_lock lock(instance_mutex);
		if (!instance.toOpt().has_value())
			throw TemplateEvaluationException("TemplateRegistry instance not initialized.");
		return *instance.toOpt().value();
	}

	void TemplateRegistrySingleton::setInstance(Box<TemplateRegistryProvider>&& provider) {
		std::scoped_lock lock(instance_mutex);
		if (instance.toOpt().empty())
			instance = base::makeBox<TemplateRegistrySingleton>(std::move(provider));
	}

	void TemplateRegistrySingleton::setNewInstance(Box<TemplateRegistryProvider>&& provider) {
		std::scoped_lock lock(instance_mutex);
		instance = base::makeBox<TemplateRegistrySingleton>(std::move(provider));
	}

	template_file::DiagnosticTemplate& TemplateRegistrySingleton::loadTemplate(
		const dia_args::Metadata& metadata
	) {
		std::scoped_lock lock(instance_mutex);
		std::string      key = metadata.type + "/" + metadata.family + "/" + metadata.name;

		if (auto cached = cache.atMaybe(key); cached.has_value()) return *cached.value();


		auto str_content_opt = provider->loadTemplate(key);
		if (!str_content_opt.has_value()) {
			throw TemplateEvaluationException(base::strConcat(
				"Template not found: type='",
				metadata.type,
				"', family='",
				metadata.family,
				"', name='",
				metadata.name,
				"'."
			));
		}
		string_view_streambuf buf(str_content_opt.value());
		std::istream          is(&buf);
		YAML::Node            yaml_node = YAML::Load(is);
		try {
			auto diagnostic_template = template_file::DiagnosticTemplate::fromYaml(yaml_node);

			if (!checkMetadataMatch(diagnostic_template, metadata)) {
				throw ParsingTemplateFileError(
					"Loaded template metadata does not match requested metadata."
				);
			}

			cache.put(key, std::move(diagnostic_template));
			return *cache.at(key);
		} catch (const ParsingTemplateFileError& e) {
			throw TemplateEvaluationException(
				base::strConcat("Error parsing template '", key, "': ", e.what())
			);
		}
	}

}
