#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Class implements list.
	 */
	class ImplementsList final:
		  public List<UniversalExprHolder, internal::NameGetters::inheritanceList> {
	public:
		explicit ImplementsList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<ImplementsList> parse(LangParserState& state);

		~ImplementsList() final = default;
	};
}
