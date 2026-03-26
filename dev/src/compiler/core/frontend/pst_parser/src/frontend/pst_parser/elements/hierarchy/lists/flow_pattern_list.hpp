
#pragma once

#include "../not_statements/patterns/flow_pattern.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief List of flow patterns, used in tuple and deconstructor patterns.
	 */
	class FlowPatternList final: public List<FlowPattern, internal::NameGetters::flowPatternList> {
	public:
		explicit FlowPatternList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::FlowPatternList;
		}

		static MBox<FlowPatternList> parse(LangParserState& state);

		~FlowPatternList() final = default;
	};
}
