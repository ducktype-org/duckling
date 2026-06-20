#pragma once

#include "context_options.hpp"

#include <hashing/add_to_hash.hpp>
#include <token_parser_core/parser_state.hpp>

namespace pst {
	class LangParserState;

	class LangParserContext final: public tpc::ParserContext {
	public:
		friend class LangParserState;

		base::StrID    class_name;
		BlockOrderType block_order;
		StmtContext    stmt_context;

		LangParserContext(
			base::StrID class_name, BlockOrderType block_order, StmtContext stmt_context
		):
			  class_name(class_name),
			  block_order(block_order),
			  stmt_context(stmt_context) {}

		LangParserContext(CRef<LangParserContext> other):
			  class_name(other->class_name),
			  block_order(other->block_order),
			  stmt_context(other->stmt_context) {}

		static Box<LangParserContext> programBaseContext() {
			return base::makeBox<LangParserContext>(
				base::StrID(""), BlockOrderType::Unordered, StmtContext::Normal
			);
		}

		static Box<LangParserContext> scriptBaseContext() {
			return base::makeBox<LangParserContext>(
				base::StrID(""), BlockOrderType::Ordered, StmtContext::Normal
			);
		}

		[[nodiscard]]
		Box<tpc::ParserContext> copy() const override {
			return base::makeBox<LangParserContext>(class_name, block_order, stmt_context);
		}

		friend constexpr void addToHash(
			hashing::hash_algorithm auto& h, const LangParserContext& ctx
		) noexcept {
			addToHash(h, ctx.class_name);
			addToHash(h, ctx.block_order);
		}
	};
}
