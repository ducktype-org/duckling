#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final:
		  public List<ImplementsElementExprHolder, internal::NameGetters::inheritanceList> {
	public:
		explicit ImplementsList(const LangParserState& state): List(state) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};
}
