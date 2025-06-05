#pragma once

#include "binary_operator.hpp"
#include "preamble.hpp"

namespace pst::expr {
	/**
	 * @brief Logical `or` operator.
	 */
	class LogicOr final: public BinaryOperator {
		using Lower = LogicAnd;
		using Self  = LogicOr;

	public:
		explicit LogicOr(const dia::SourcePosition& position):
			  BinaryOperator(position, lang_def::keywordToStr(lang_def::Keyword::Or), 760) {}

		static MBox<ExprElement> parse(LangParserState& state, i64 length);

		~LogicOr() override = default;
	};
}
