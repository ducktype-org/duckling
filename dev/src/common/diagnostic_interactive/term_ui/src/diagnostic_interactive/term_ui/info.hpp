#pragma once
#include "code_section.hpp"
#include "component_pieces.hpp"

#include <variant>

namespace term_ui {

	/**
	 * @brief The class responsible for displaying one info.
	 *
	 * It consists of a main section and secondary sections to be displayed
	 * in the specified order.
	 */
	struct Info {
		/**
		 * Note that the term-ui is a static UI, so the text section
		 * is serialized to a simple string (all interactive metadata
		 * is discarded).
		 */
		using TextSection = std::string;
		using TextOrCode  = std::variant<TextSection, CodeSection>;

		// The type of the info.
		StyleType type;
		/**
		 * @brief The info template ID.
		 *
		 * May be displayed preceeding the header message (main section)
		 * of this info.
		 */
		u32 id;
		// The main section of this info (this is the header message).
		TextSection main_section;
		/**
		 * @brief The secondary sections of this info (in a specific order).
		 *
		 * Sections may be text or code.
		 */
		std::vector<TextOrCode> sections;

		// Construct the info from the corresponding data provided by
		// the view manager.
		Info(const view::Info& info);

		/**
		 * @brief Print this info to output stream.
		 *
		 * @param out The output stream.
		 */
		void print(std::ostream& out) const;


		// Construct a new info without a gRPC object. For testing only.
		Info(
			StyleType          type,
			u32                id,
			const std::string& message,
			const std::string& description,
			const CodeSection& code
		);
	};

}
