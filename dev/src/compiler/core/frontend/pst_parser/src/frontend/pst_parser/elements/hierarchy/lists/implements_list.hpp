#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	namespace {
		using ImplementsParentList = List<ImplementsElementExprHolder, internal::NameGetters::inheritanceList>;
	}

	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final: public ImplementsParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(ImplementsList, ImplementsParentList);
	public:
		explicit ImplementsList(const LangParserState& state): List(state) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};
}
