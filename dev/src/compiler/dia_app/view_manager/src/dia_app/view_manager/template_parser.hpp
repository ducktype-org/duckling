#pragma once
#include "dia_parser.hpp"
#include "utils.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace dia_app {
namespace message_template {

	struct TemplateElement;
	using TemplatePtr = std::shared_ptr<TemplateElement>;

	/**
	 * @brief A pointer message template. Part of `InfoTemplate`.
	 * 
	 */
	struct PointerMessage {
		// The lower this number, the higher the priority.
		u32 priority;
		// Type of the message.
		InfoType type;
		// Content of the message.
		TemplatePtr message;

		PointerMessage();
		PointerMessage(const YAML::Node& msg);
	};

	/**
	 * @brief A parsed info template.
	 * 
	 * Upon conversion to `Info`, all sections of this template are evaluated
	 * with the parameters passed down from the compiler.
	 * 
	 */
	struct InfoTemplate {
	private:
		// The parameters declared for this template.
		YAML::Node declared_params;
		// The parameters declared for this template's explore edges.
		base::HashMap<std::string, YAML::Node> declared_explore_edges_params;
	public:
		// The metadata identifying this info template.
		Metadata metadata;
		/**
		 * @brief Macros declared in this template.
		 * 
		 * The macros are non-parametrized, so you can think of them
		 * as shorthand aliases for more complicated template constructions.
		 */
		base::HashMap<std::string, TemplatePtr> macros;

		// Different sections of this info template.

		TemplatePtr                                 header_message;
		base::HashMap<std::string, PointerMessage> 	pointer_messages;
		TemplatePtr                                 description;
		base::HashMap<std::string, TemplatePtr>		explore_edges;

		InfoTemplate(const ShortMetadata &short_metadata);

		// Verify the validity of the provided template parameters.
		void verify(const dia_file::InfoParams& params);

	private:
		// Verify that all declared parameters have been provided and that
		// no undeclared parameters were provided.
		static void verify_params(
			const YAML::Node& declared_params,
			const base::HashMap<std::string, dia_file::DisplayPtr>& provided_params
		);
	};

}  // namespace message_template
}  // namespace dia_app
