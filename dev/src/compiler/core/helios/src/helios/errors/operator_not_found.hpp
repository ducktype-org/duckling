#pragma once
#include <diagnostic/interactive_message.hpp>
#include <helios/hout/elements/expr.hpp>

namespace dia {
	using compiler::helios::code::Expr;

	// class OperatorNotFound: public InteractiveMessage {
	// 	class Params: public ContentParams {
	// 	public:
	// 		base::StrID op;
	// 		Box<Expr>   lhs;
	// 		Box<Expr>   rhs;

	// 		Params(base::StrID op, Box<Expr> lhs, Box<Expr> rhs):
	// 			  ContentParams(),
	// 			  op(op),
	// 			  lhs(std::move(lhs)),
	// 			  rhs(std::move(rhs)) {
	// 			auto& lhs_type = this->lhs->expression_type;
	// 			auto& rhs_type = this->rhs->expression_type;
	// 			types.insert(lhs_type.getType());
	// 			types.insert(rhs_type.getType());
	// 		}

	// 		json tojson() override;
	// 	};

	// 	class Content: public InteractiveContent {
	// 	public:
	// 		Content(
	// 			Box<ContentParams>                  params,
	// 			dia::SourcePosition                 position,
	// 			pst::AccessLocked<pst::LangElement> pst,
	// 			query::Context&                     ctx
	// 		):
	// 			  InteractiveContent(
	// 				  ContentType::ERROR,
	// 				  "type_check",
	// 				  "no_match_2op",
	// 				  std::move(params),
	// 				  base::makeBox<InteractiveCode>(
	// 					  position, pst, ctx, dia::PointerMessage{ "cause", position }
	// 				  )
	// 			  ) {}
	// 	};

	// public:
	// 	OperatorNotFound(
	// 		dia::SourcePosition                 position,
	// 		base::StrID                         op,
	// 		Box<Expr>                           lhs,
	// 		Box<Expr>                           rhs,
	// 		pst::AccessLocked<pst::LangElement> pst,
	// 		query::Context&                     ctx
	// 	):
	// 		  InteractiveMessage(
	// 			  base::makeBox<Content>(
	// 				  base::makeBox<Params>(op, std::move(lhs), std::move(rhs)), position, pst, ctx
	// 			  ),
	// 			  std::map<std::string, Box<dia::InteractiveNote>>{}
	// 		  ) {}
	// };
}
