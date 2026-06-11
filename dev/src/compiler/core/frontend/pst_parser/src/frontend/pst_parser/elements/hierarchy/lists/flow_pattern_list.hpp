
#pragma once

#include "../not_statements/patterns/flow_pattern.hpp"
#include "preamble.hpp"

namespace pst {
	namespace {
		using FlowPatternParentList = List<FlowPattern, internal::NameGetters::flowPatternList>;
	}

	/**
	 * @brief List of flow patterns, used in tuple and deconstructor patterns.
	 */
	class FlowPatternList final: public FlowPatternParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(FlowPatternList, FlowPatternParentList);
		CLONE_SUBELEMENTS();
	public:
		explicit FlowPatternList(const LangParserState& state): List(state) {
			this->element_kind = ElementKind::FlowPatternList;
		}

		static MBox<FlowPatternList> parse(LangParserState& state);

		~FlowPatternList() final = default;
	};
}
