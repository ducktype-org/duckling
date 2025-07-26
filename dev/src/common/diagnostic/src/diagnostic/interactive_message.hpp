#pragma once

#include "common.hpp"
#include "diagnostic/interactive_code.hpp"
#include "interactive_content.hpp"
#include "query_framework/query_int.hpp"

#include <helios/hout/elements/expr.hpp>
#include <json/json.hpp>
#include <pst_parser/elements/hierarchy/not_statements/expr_element.hpp>

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
				Box<ContentParams>                  params,
				dia::SourcePosition                 position,
				pst::AccessLocked<pst::LangElement> pst,
				query::Context&                     ctx
			):
				  InteractiveContent(
					  ContentType::ERROR,
					  "type_check",
					  "no_match_2op",
					  std::move(params),
					  base::makeBox<InteractiveCode>(
						  position, pst, ctx, dia::pointer_message{ "cause", position }
					  )
				  ) {}
		};

	public:
		OperatorNotFound(
			dia::SourcePosition                 position,
			base::StrID                         op,
			Box<Expr>                           lhs,
			Box<Expr>                           rhs,
			pst::AccessLocked<pst::LangElement> pst,
			query::Context&                     ctx
		):
			  InteractiveMessage(
				  base::makeBox<Content>(
					  base::makeBox<Params>(op, std::move(lhs), std::move(rhs)), position, pst, ctx
				  ),
				  std::vector<Box<dia::InteractiveNote>>{}
			  ) {}
	};

	class ParseError: public dia::InteractiveMessage {
	public:
		ParseError(std::string family, std::string name, dia::SourcePosition position):
			  dia::InteractiveMessage(
				  makeBox<InteractiveContent>(
					  ContentType::ERROR,
					  family,
					  name,
					  base::makeBox<EmptyParams>(),
					  base::makeBox<SimpleCode>(position, pointer_message{ "here", position })
				  ),
				  std::vector<Box<dia::InteractiveNote>>{}
			  ) {}
	};

	class RoundBracket: public ParseError {
	public:
		RoundBracket(dia::SourcePosition position): ParseError("parse", "for_round_bracket", position) {}
	};

	class TODOError: public InteractiveMessage {
		class Params: public ContentParams {
			const std::string message;

		public:
			Params(const std::string& message): message(message) {}

			json tojson() override { return { "message", message }; }
		};

		TODOError(dia::SourcePosition position, const std::string& message):
			  dia::InteractiveMessage(
				  makeBox<InteractiveContent>(
					  ContentType::ERROR,
					  "misc",
					  "todo",
					  base::makeBox<Params>(message),
					  base::makeBox<SimpleCode>(position, pointer_message{ "here", position })
				  ),
				  std::vector<Box<dia::InteractiveNote>>{}
			  ) {}
	};
}
