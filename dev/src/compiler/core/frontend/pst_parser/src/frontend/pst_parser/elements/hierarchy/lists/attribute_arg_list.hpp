#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final:
		  public List<UniversalExprHolder, internal::NameGetters::attributeArgList> {
	public:
		explicit AtrArgList(const LangParserState& state): List(state) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
