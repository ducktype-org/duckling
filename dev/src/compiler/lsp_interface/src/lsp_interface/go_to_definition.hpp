/**
 * @file go_to_definition.hpp
 * @brief Go to definition definition
 * @TODO: #3604 bring this back
 */

#pragma once

#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/lang_parser_element.hpp>

#include <string>
#include <utility>

namespace lsp {
	struct Definition final {
		std::string             uri;
		std::pair<usize, usize> start;  // line, char
		std::pair<usize, usize> end;    // line, char

		std::string toJSON();

		Definition(const pst::LangElement*);
	};

	/**
	 * @brief Find the element at the given offset in a subtree with a given root.
	 * @param root The root element of a subtree to search in.
	 * @param offset The offset to search for. Offset is a index in the source file.
	 * @param include_symbold_before_offset If true, include in search symbols that end at offset - 1.
	 * @return The element at the given offset.
	 * @note This function will find the minimal element whose source position contains the given
	 * offset. If offset is not contained in the root element, it will return the root element
	 * itself.
	 */
	pst::AccessLocked<pst::LangElement> findElement(
		pst::AccessLocked<pst::LangElement> root, usize offset, bool include_symbold_before_offset
	);

	/**
	 * @brief Find the definition of a given element.
	 * @param element The element to find the definition for.
	 * @return The definition of the element.
	 * @note The element must be able to be cast to an ExprElement
	 */
	base::Optional<Definition> findDefinition(
		pst::AccessLocked<pst::LangElement> element, query::Context& ctx
	);
}
