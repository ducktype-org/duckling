#pragma once

#include <token_parser_core/parser_state.hpp>
#include "ordering.hpp"

namespace pst {
	class LangParserState;

	class LangParserContext: public tpc::ParserContext {
	private:
		friend class LangParserState;		

		base::StrID class_name;
		BlockOrderType block_order;

	public:
		LangParserContext(base::StrID class_name, BlockOrderType block_order): class_name(class_name), block_order(block_order) {}

		static Box<LangParserContext> programBaseContext() {
			return base::makeBox<LangParserContext>(base::StrID(""), BlockOrderType::Unordered);
		}

		static Box<LangParserContext> scriptBaseContext() {
			return base::makeBox<LangParserContext>(base::StrID(""), BlockOrderType::Ordered);
		}

		[[nodiscard]]
		Box<LangParserContext> copy() const {
			return base::makeBox<LangParserContext>(class_name, block_order);
		}
	};
}