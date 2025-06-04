#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Template initialization list.
	 */
	class TemplateList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::templateList> {
	public:
		explicit TemplateList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<TemplateList> parse(LangParserState& state);

		~TemplateList() final = default;
	};
}
