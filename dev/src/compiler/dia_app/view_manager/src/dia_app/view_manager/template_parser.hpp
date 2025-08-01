#pragma once
#include "dia_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace dia_app {
	namespace message_template {

		struct TemplateElement;
		using Ptr = std::shared_ptr<TemplateElement>;
		Ptr parse(const YAML::Node& msg);

#define MAX_PRIORITY_UINT 1'000'000'000

		struct PointerMessage {
			// The lower this number, the higher the priority.
			u32 priority;
			// Type of the message: error | warning | note | hint | docs.
			std::string type;
			// Content of the message.
			Ptr message;

			PointerMessage() {}

			PointerMessage(const YAML::Node& msg): priority(MAX_PRIORITY_UINT) {
				// require content
				assert(msg["content"]);
				message = parse(msg["content"]);

				// require type
				assert(msg["type"] && msg["type"].IsScalar());
				type = msg["type"].as<std::string>();

				if (msg["priority"]) {
					priority = msg["priority"].as<unsigned>();
					assert(priority <= MAX_PRIORITY_UINT);
				}
			}
		};

		struct TemplateData {
			Metadata metadata;
			// Declared macros.
			base::HashMap<std::string, Ptr> macros;

			// -- Message parts --
			Ptr                                   		header_message;
			base::HashMap<std::string, PointerMessage> 	pointer_messages;
			Ptr                                   		description;
			base::HashMap<std::string, Ptr>			  	explore_edges;

			TemplateData(const dia_file::ParamData& params) {
				// Fetch the message template.
				std::string   filename = params.metadata.getPath();
				std::ifstream file(filename);
				if (!file.is_open()) {
					std::cerr << "Failed to open message template: " << filename << std::endl;
					// This may not be a bug, but an OS problem on user side,
					// so do not assert. TODO: Exception must be handled.
					throw TemplateFileNotFoundException();
				}

				YAML::Node template_yaml = YAML::Load(file);

				// Parse metadata.
				assert(template_yaml["metadata"]);
				metadata = Metadata(template_yaml["metadata"]);
				assert(metadata.sameAs(params.metadata));

				// Parse macros.
				const YAML::Node& macros_node = template_yaml["macros"];
				if (macros_node && macros_node.IsMap()) {
					for (const auto& it: macros_node) {
						const std::string key = it.first.as<std::string>();
						macros.put(key, parse(it.second));
					}
				}

				// Parse parameters.
				verify_params(template_yaml["params"], params.params);

				// Parse explore edge micro templates.
				const YAML::Node& edge_templates = template_yaml["explore_edges"];
				if (edge_templates && edge_templates.IsMap()) {
					// Parse all edge templates.
					for (const auto& it: edge_templates) {
						const std::string key = it.first.as<std::string>();
						assert(it.second["content"]);
						
						explore_edges.put(key, parse(it.second["content"]));

						// Verify params of all users of this edge template.
						for (const auto& e: params.explore_edges) {
							if (e.name == key) {
								verify_params(it.second["params"], e.params);
							}
						}
					}
				}

				// Parse message parts.
				// - header message
				assert(template_yaml["header_message"]);
				header_message = parse(template_yaml["header_message"]);

				// - pointer messages
				if (template_yaml["pointer_messages"]) {
					const YAML::Node& pm_node = template_yaml["pointer_messages"];
					assert(pm_node.IsMap());
					for (const auto& it: pm_node) {
						const std::string key = it.first.as<std::string>();
						pointer_messages.put(key, PointerMessage(it.second));
					}
				}

				// - description
				if (template_yaml["description"]) description = parse(template_yaml["description"]);
			}

		private:
			// Verify whether there are enough provided params and that no surplus params
			// were provided.
			void verify_params(const YAML::Node& declared_params, const base::HashMap<std::string, dia_file::Ptr>& provided_params) const {
				if (declared_params && declared_params.IsMap()) {
					// Did not provide more than available.
					for (const auto& it: provided_params) assert(declared_params[it.first]);
					// Did not provide fewer than necessary.
					for (const auto& it: declared_params) {
						const YAML::Node& val = it.second;
						if (!val["optional"] || !val["optional"].as<bool>())
							assert(provided_params.count(it.first.as<std::string>()));
					}
				}
			}
		};

	}  // namespace message_template
}  // namespace dia_app
