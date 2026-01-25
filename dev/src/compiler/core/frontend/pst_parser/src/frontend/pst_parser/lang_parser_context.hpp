#pragma once

#include "ordering.hpp"

#include <token_parser_core/parser_state.hpp>

namespace pst {
	class LangParserState;

	class LangParserContext final: public tpc::ParserContext {
	public:
		friend class LangParserState;

		base::StrID    class_name;
		BlockOrderType block_order;

		LangParserContext(base::StrID class_name, BlockOrderType block_order):
			  class_name(class_name),
			  block_order(block_order) {}

		static Box<LangParserContext> programBaseContext() {
			return base::makeBox<LangParserContext>(base::StrID(""), BlockOrderType::Unordered);
		}

		static Box<LangParserContext> scriptBaseContext() {
			return base::makeBox<LangParserContext>(base::StrID(""), BlockOrderType::Ordered);
		}

		[[nodiscard]]
		Box<tpc::ParserContext> copy() const override {
			return base::makeBox<LangParserContext>(class_name, block_order);
		}
	};
}
