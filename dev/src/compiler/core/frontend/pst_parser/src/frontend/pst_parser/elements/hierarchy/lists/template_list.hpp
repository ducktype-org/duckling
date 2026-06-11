#pragma once

#include "preamble.hpp"

namespace pst {
	namespace {
		using TemplateParentList = List<UniversalExprHolder, internal::NameGetters::templateList>;
	}

	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final: public TemplateParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(TemplateList, TemplateParentList);
	public:
		explicit TemplateList(const LangParserState& state): List(state) {}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
