
#pragma once

#include "../not_statements/patterns.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief List of flow patterns, used in tuple and deconstructor patterns.
	 */
	class FlowPatternList final: public List<FlowPattern, internal::NameGetters::flowPatternList> {
	public:
		explicit FlowPatternList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::FlowPatternList;
		}

		static MBox<FlowPatternList> parse(LangParserState& state);

		~FlowPatternList() final = default;
	};
}
