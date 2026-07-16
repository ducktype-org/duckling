#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	/**
	 * @brief c++-like class constructor initialization list.
	 *
	 * @note It's probably going to be deprecated
	 */
	class InitList final: public List<UniversalExprHolder, internal::NameGetters::classInitList> {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(InitList, List);
		CLONE_SUBELEMENTS();

	public:
		explicit InitList(LangElementConstructionArgument state): List(state) {}

		static MBox<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};
}
