#pragma once

#include "../expr_holders.hpp"
#include "preamble.hpp"

namespace pst {
	namespace {
		using InitParentList = List<UniversalExprHolder, internal::NameGetters::classInitList>;
	}

	/**
	 * @brief c++-like class constructor initialization list.
	 *
	 * @note It's probably going to be deprecated
	 */
	class InitList final: public InitParentList {
		SIMPLE_FINAL_ELEMENT_CLASS_PREAMBLE(InitList, InitParentList);
		CLONE_SUBELEMENTS();
	public:
		explicit InitList(const LangParserState& state): List(state) {}

		static MBox<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};
}
