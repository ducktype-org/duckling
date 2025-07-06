#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Attribute argument list.
	 */
	class AtrArgList final:
		  public List<UniversalExprHolder, internal::NameGetters::attributeArgList> {
	public:
		explicit AtrArgList(const dia::SourcePosition& pos): List(pos) {}

		static MBox<AtrArgList> parse(LangParserState& state);

		~AtrArgList() final = default;
	};
}
