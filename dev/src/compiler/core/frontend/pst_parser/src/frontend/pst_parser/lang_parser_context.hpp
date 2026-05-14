#pragma once

#include "ordering.hpp"

#include <hashing/add_to_hash.hpp>
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

		LangParserContext(CRef<LangParserContext> other):
			  class_name(other->class_name),
			  block_order(other->block_order) {}

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

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const LangParserContext& ctx
		) noexcept {
			addToHash(h, ctx.class_name);
			addToHash(h, ctx.block_order);
		}
	};
}
