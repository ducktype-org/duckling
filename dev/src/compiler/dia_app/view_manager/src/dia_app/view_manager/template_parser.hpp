#pragma once
#include "dia_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace dia_app {
namespace message_template {

	struct TemplateElement;
	using TemplatePtr = std::shared_ptr<TemplateElement>;
	TemplatePtr parse(const YAML::Node& msg);

	#define MAX_PRIORITY_UINT 1'000'000'000

	struct PointerMessage {
		// The lower this number, the higher the priority.
		u32 priority;
		// Type of the message: error | warning | note | hint | docs.
		std::string type;
		// Content of the message.
		TemplatePtr message;

		PointerMessage();
		PointerMessage(const YAML::Node& msg);
	};

	struct TemplateData {
		Metadata metadata;
		// Declared macros.
		base::HashMap<std::string, TemplatePtr> macros;

		// -- Message parts --
		TemplatePtr                                 header_message;
		base::HashMap<std::string, PointerMessage> 	pointer_messages;
		TemplatePtr                                 description;
		base::HashMap<std::string, TemplatePtr>		explore_edges;

		TemplateData(const dia_file::InfoParams& params);

	private:
		// Verify whether there are enough provided params and that no surplus params
		// were provided.
		void verify_params(
			const YAML::Node& declared_params,
			const base::HashMap<std::string, dia_file::DisplayPtr>& provided_params
		) const;
	};

}  // namespace message_template
}  // namespace dia_app
