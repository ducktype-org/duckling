#pragma once

#include "common.hpp"
#include "interactive_content.hpp"
#include "query_framework/query_int.hpp"

#include <helios/hout/elements/expr.hpp>
#include <json/json.hpp>

#include "base/string_id.hpp"
#include <base/box.hpp>

#include <vector>

namespace dia {
	using nlohmann::json;

	class InteractiveMessage {
	private:
		Box<dia::InteractiveContent>           content;
		std::vector<Box<dia::InteractiveNote>> notes;

	public:
		InteractiveMessage(
			Box<dia::InteractiveContent> content, std::vector<Box<dia::InteractiveNote>>&& notes
		):
			  content(std::move(content)),
			  notes(std::forward<decltype(notes)>(notes)) {}

		friend void to_json(json& j, const InteractiveMessage& message) { j = message.tojson(); }

		json tojson() const;
	};

	class ExampleMessage: public InteractiveMessage {
	public:
		ExampleMessage():
			  InteractiveMessage(
				  base::makeBox<ExampleContent>(), std::vector<Box<dia::InteractiveNote>>()
			  ) {}
	};

	using compiler::helios::code::Expr;

	class OperatorNotFound: public InteractiveMessage {
		class Params: public ContentParams {
		public:
			base::StrID op;
			Box<Expr>   lhs;
			Box<Expr>   rhs;

			Params(base::StrID op, Box<Expr> lhs, Box<Expr> rhs):
				  ContentParams(),
				  op(op),
				  lhs(std::move(lhs)),
				  rhs(std::move(rhs)) {
				auto& lhs_type = this->lhs->expression_type;
				auto& rhs_type = this->rhs->expression_type;
				types.insert(lhs_type.getType());
				types.insert(rhs_type.getType());
			}

			json tojson() override;
		};

		class Content: public InteractiveContent {
		public:
			Content(
				Box<ContentParams>            params,
				dia::SourcePosition           position,
				pst::Access<pst::LangElement> pst,
				query::Context&               ctx
			):
				  InteractiveContent(
					  ContentType::ERROR,
					  "type_check",
					  "no_match_2op",
					  std::move(params),
					  position,
					  pst,
					  ctx,
					  { "cause", position }
				  ) {}
		};

	public:
		OperatorNotFound(
			dia::SourcePosition           position,
			base::StrID                   op,
			Box<Expr>                     lhs,
			Box<Expr>                     rhs,
			pst::Access<pst::LangElement> pst,
			query::Context&               ctx
		):
			  InteractiveMessage(
				  base::makeBox<Content>(
					  base::makeBox<Params>(op, std::move(lhs), std::move(rhs)), position, pst, ctx
				  ),
				  std::vector<Box<dia::InteractiveNote>>{}
			  ) {}
	};

}
