#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final:
		  public List<ImplementsElementExprHolder, internal::NameGetters::inheritanceList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ImplementsList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit ImplementsList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::InheritanceList;
		}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};
}
