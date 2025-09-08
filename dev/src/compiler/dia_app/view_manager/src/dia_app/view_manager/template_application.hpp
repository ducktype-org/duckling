#pragma once
#include "dia_parser.hpp"
#include "template_elements.hpp"
#include "template_parser.hpp"
#include "utils.hpp"

namespace dia_app {
	// Forward declaration.
	struct ViewConstructor;

	namespace message_template {

		/**
		 * @brief An explore edge representation after applying a message template.
		 *
		 */
		struct ExploreEdge {
			// The description to be displayed at this explore edge.
			dia_file::DisplayPtr description;
			// The id of the info this edge is referring to.
			InfoID info_id;

			ExploreEdge(dia_file::DisplayPtr description, InfoID info_id);

			/**
			 * @brief Evaluate the description text of this explore edge.
			 *
			 * @param vc View constructor reference for fetching purposes
			 * @return std::string
			 */
			std::string getDescription(ViewConstructor& vc);
		};

		/**
		 * @brief A pointer message representation after applying a message
		 * template.
		 *
		 */
		struct DisplayPointerMessage {
			// The priority of this pointer message. The lower the value,
			// the higher the priority.
			u32 priority;
			// The type of this pointer message.
			InfoType type;
			// The content of this pointer message.
			dia_file::DisplayPtr message;

			DisplayPointerMessage();
			DisplayPointerMessage(const PointerMessage& msg, ToDisplayVisitor& v);
		};

		/**
		 * @brief An info representation after applying an info template.
		 *
		 */
		struct Info {
			// The metadata identifying this info's template.
			Metadata metadata;

			// Different sections of this info.

			dia_file::DisplayPtr                              header_message;
			base::Optional<dia_file::CodeData>                code;
			base::HashMap<std::string, DisplayPointerMessage> pointer_messages;
			dia_file::DisplayPtr                              description;
			std::vector<ExploreEdge>                          explore_edges;

			Info(
				ViewConstructor&            vc,
				const InfoTemplate&         info_template,
				const dia_file::InfoParams& info
			);

		private:
			void verify_code() const;
		};

		/**
		 * @brief Apply a message template identified and parametrized by `info`
		 * and return the resulting info.
		 *
		 * @param vc View constructor reference for potential fetching of the
		 * `case ... of ...` template elements
		 * @param info Info parameters from the diagnostic file
		 * @return Info
		 */
		Info apply(ViewConstructor& vc, const dia_file::InfoParams& info);

	}  // namespace message_template
}  // namespace dia_app
