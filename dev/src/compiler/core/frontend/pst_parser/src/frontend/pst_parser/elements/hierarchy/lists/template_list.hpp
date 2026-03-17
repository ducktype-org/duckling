#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, internal::NameGetters::templateList> {
	public:
		explicit TemplateList(const LangParserState& state): List(state) {}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
