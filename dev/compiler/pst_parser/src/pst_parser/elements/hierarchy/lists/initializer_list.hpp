#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief c++-like class constructor initialization list.
	 *
	 * @note It's probably going to be deprecated
	 */
	class InitList final: public List<UniversalExprHolder, detail::NameGetters::classInitList> {
	public:
		explicit InitList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<InitList> parse(LangParserState& state);

		~InitList() final = default;
	};
}
