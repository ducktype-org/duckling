#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Call argument list.
	 */
	class CallList final:
		  public List<UniversalExprHolderLowerLevel, detail::NameGetters::callList> {
	public:
		explicit CallList(const dia::SourcePosition& pos): List(pos) {
			this->element_kind = ElementKind::CallList;
		}

		static MBox<CallList> parse(LangParserState& state);

		~CallList() final = default;
	};
}
