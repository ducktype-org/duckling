#pragma once

#include "../not_statements/call_argument.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Call argument list.
	 */
	class CallList final: public List<CallArgument, internal::NameGetters::callList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(CallList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit CallList(LangElementConstructionArgument state): List(state) {
			this->element_kind = ElementKind::CallList;
		}

		static MBox<CallList> parse(LangParserState& state);

		~CallList() final = default;
	};
}
