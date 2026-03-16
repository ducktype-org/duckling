/**
 * @file semantic_tokens.cpp
 * @brief This file defines the SemanticToken class, which provides functionality to export
 * semantic tokens in JSON format.
 */
#include "semantic_tokens.hpp"

#include "frontend/pst_parser/access.hpp"
#include "frontend/pst_parser/element_kind.hpp"
#include "frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp"
#include "frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp"
#include "frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp"
#include "frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp"
#include "frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp"
#include "frontend/pst_parser/pst_visitor.hpp"
#include "helios/hout/elements/expr.hpp"
#include "helios/hout/origin.hpp"
#include "helios/symbols/query_type_of_symbol.hpp"
#include "helios/symbols/symbol_id_utils.hpp"
#include "utils.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/ls_utils/ls_utils.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include "diagnostic/source_position.hpp"
#include "lexer/token.hpp"
#include "query_framework/entry/query_entry_point.hpp"
#include <query_framework/entry/with_context_do.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <unordered_set>

namespace lsp {

	SemanticToken::SemanticToken(CRef<lexer::Token> source, StandardTokenType type):
		  source_token(source),
		  line(source->getPosition().getStartLineColumn().first - 1),
		  start_character(source->getPosition().getStartLineColumn().second - 1),
		  length(source->getPosition().getEnd() - source->getPosition().getStart() + 1),
		  type(type) {}

	SemanticToken::SemanticToken(CRef<lexer::Token> source):
		  SemanticToken(source, translateType(source->getType())) {}

	StandardTokenType SemanticToken::translateType(lexer::Token::Type type) {
		using lTT = lexer::Token::Type;
		using sT  = StandardTokenType;
		switch (type) {
		case lTT::Keyword:
			return sT::Keyword;
		case lTT::NumLiteral:
			return sT::Number;
		case lTT::String:
			return sT::String;
		case lTT::FormattedString:
			return sT::String;
		case lTT::Operator:
			return sT::Operator;
		case lTT::Comment:
			return sT::Comment;
		case lTT::TypeSpecifier:
			return sT::Type;
		case lTT::Identifier:
			// Further classification would require semantic analysis.
			return sT::Variable;
		case lTT::Sentinel:
			return sT::Operator;
			// case lTT::Special:
			// case lTT::Empty:
		// case lTT::Error: return;
		default:
			return sT::Unknown;
		}
	}

	std::string SemanticToken::toJSON() {
		std::map<std::string, std::string> result;

		result["line"]           = std::to_string(this->line);
		result["startCharacter"] = std::to_string(this->start_character);
		result["length"]         = std::to_string(this->length);
		// lst expects token type to be a number, not descriptive name
		result["tokenType"]      = std::to_string(static_cast<int8_t>(this->type));
		result["tokenModifiers"] = "0";  // @TODO Duckling LSP 2.0

		return jsonDict(result);
	}

	struct PrecalculatedSemanticToken {
		dia::SourcePosition position;
		StandardTokenType   correct_type;

		[[nodiscard]] bool isPrecalculatedFor(const CRef<lexer::Token>& token) const {
			// We should compare based on whole position, not just the end.
			return token->getPosition().getEnd() == position.getEnd();
		}
	};

	struct TokenContext {
		std::deque<PrecalculatedSemanticToken> precalculated{};
		base::Optional<StandardTokenType>      default_identifier_type{};
		bool                                   precalculate_for_children = true;
	};

	struct TokenTopLevelVisitor final: public pst::PstVisitorEmpty {
		static bool supports(pst::Access<pst::LangElement> element) {
			// For now we only support precalculation for function declarations, but this can be
			// extended in the future.
			switch (element->getElementKind()) {
			case pst::ElementKind::Import:
			case pst::ElementKind::Using:
			case pst::ElementKind::Alias:
			case pst::ElementKind::Namespace:
			case pst::ElementKind::Fun:
			case pst::ElementKind::FunDecl:
			case pst::ElementKind::Class:
			case pst::ElementKind::ClassMethod:
			case pst::ElementKind::ClassField:
			case pst::ElementKind::Param:
			case pst::ElementKind::CallArgument:
			case pst::ElementKind::Const:
				return true;
			default:
				return false;
			}
		}

		static void precalculateTokens(pst::Access<pst::LangElement> element, TokenContext& ctx) {
			TokenTopLevelVisitor visitor(ctx.precalculated);
			element->acceptVisitor(visitor);
		}

		explicit TokenTopLevelVisitor(std::deque<PrecalculatedSemanticToken>& identified_tokens):
			  identified_tokens(identified_tokens) {}

		std::deque<PrecalculatedSemanticToken>& identified_tokens;


		~TokenTopLevelVisitor() override = default;

		void out(dia::SourcePosition position, StandardTokenType correct_type) {
			identified_tokens.push_back({ position, correct_type });
		}

		void visitImport(pst::Access<pst::Import> elem) override {
			auto chain_locked = elem->getImportChain();
			if_opt_none(chain_locked.illegalAccess()) return;
			auto chain = chain_locked.illegalAccess().value();
			auto names = chain->getNames();
			std::cerr << "Import names: ";

			for (auto name: names) {
				out(name.position, StandardTokenType::Namespace);
				std::cerr << name.value.strView() << " ";
			}
		}

		void visitUsing(pst::Access<pst::Using>) override {
			// For now empty
		}

		void visitAlias(pst::Access<pst::Alias>) override {
			// Same as using
		}

		void visitNamespace(pst::Access<pst::Namespace> elem) override {
			out(elem->getNameIdent().position, StandardTokenType::Namespace);
		}

		void visitClass(pst::Access<pst::Class> elem) override {
			out(elem->getNameIdent().position, StandardTokenType::Class);
		}

		void visitFun(pst::Access<pst::Fun> elem) override {
			out(elem->getNameIdentifier().position, StandardTokenType::Function);
		}

		void visitFunDecl(pst::Access<pst::FunDecl> elem) override {
			out(elem->getNameIdentifier().position, StandardTokenType::Function);
		}

		void visitVariable(pst::Access<pst::Variable> elem) override {
			out(elem->getNameIdent().position, StandardTokenType::Variable);
		}

		void visitMethod(pst::Access<pst::Method> elem) override {
			out(elem->getNameIdentifier().position, StandardTokenType::Method);
		}

		void visitField(pst::Access<pst::Field> elem) override {
			out(elem->getNameIdent().position, StandardTokenType::Property);
		}

		void visitParam(pst::Access<pst::Param> elem) override {
			out(elem->getNameIdent().position, StandardTokenType::Parameter);
		}

		void visitCallArgument(pst::Access<pst::CallArgument> elem) override {
			if_opt_some(elem->getArgName().value, _) {
				out(elem->getArgName().position, StandardTokenType::Parameter);
			}
		}

		void visitConst(pst::Access<pst::Const> elem) override {
			StandardTokenType token_type = StandardTokenType::Variable;
			query::utils::withContextDo([&](query::Context& ctx) {
				auto symbol = compiler::helios::ls::getSymbolOfStmt(ctx, elem);
				if (symbol.hasFailed()) return;
				auto qresult
					= ctx.query<compiler::helios::QueryTypeOfSymbol>({ symbol.valueOrPanic() });
				if (qresult->hasFailed()) return;
				auto symbol_type = qresult->valueOrPanic();
				if (symbol_type.getType().getKind() == compiler::tsh::Kind::Meta)
					token_type = StandardTokenType::Type;
			});
			out(elem->getNameIdent().position, token_type);
		}
	};

	using namespace compiler::helios;

	class TokenHoutExprVisitor final: public code::HoutExprVisitorEmpty {

	public:
		static bool supports(pst::Access<pst::LangElement> element) {
			if (element.dynamicCast<pst::ExprElement>().has_value()
			    && element.dynamicCast<pst::expr::Assignment>().empty()) {
				return true;
			}
			return false;
		}

		static void precalculateTokens(pst::Access<pst::LangElement> element, TokenContext& result) {
			auto expr_element = element.dynamicCast<pst::ExprElement>().value();

			result.precalculate_for_children = false;

			MCRef<query::QResult<Box<code::Expr>>> hout_expr_result;
			query::utils::withContextDo([&](query::Context& ctx) {
				hout_expr_result = compiler::helios::ls::getHoutExpr(ctx, expr_element);
			});
			if (hout_expr_result->hasFailed()) return;

			auto&                hout_expr = hout_expr_result->valueOrPanic();
			TokenHoutExprVisitor visitor(result.precalculated);
			hout_expr->acceptVisitor(visitor);
			result.default_identifier_type.emplace(StandardTokenType::Namespace);
		}

		explicit TokenHoutExprVisitor(std::deque<PrecalculatedSemanticToken>& identified_tokens):
			  identified_tokens(identified_tokens) {}

		std::deque<PrecalculatedSemanticToken>& identified_tokens;

		~TokenHoutExprVisitor() override = default;

		void out(dia::SourcePosition pos, StandardTokenType correct_type) {
			identified_tokens.push_back({ pos, correct_type });
		}

		void out(CRef<lexer::Token> token, StandardTokenType correct_type) {
			identified_tokens.push_back({ token->getPosition(), correct_type });
		}

		void visitIdentifierExpr(const code::IdentifierExpr& elem) override {
			auto maybe_position = elem.origin.getSourcePosition();
			if_opt_none(maybe_position) return;
			auto position = maybe_position.value();

			switch (kind(elem.symbol)) {
			case SymbolKind::Namespace:
				out(position, StandardTokenType::Namespace);
				return;
			case SymbolKind::Function:
			case SymbolKind::FunctionDeclaration:
				out(position, StandardTokenType::Function);
				return;
			case SymbolKind::Method:
				out(position, StandardTokenType::Method);
				return;
			case SymbolKind::Parameter:
				out(position, StandardTokenType::Parameter);
				return;
			case SymbolKind::Class:
				out(position, StandardTokenType::Class);
				return;
			case SymbolKind::Field:
				out(position, StandardTokenType::Property);
				return;
			case SymbolKind::Const: {
				auto qresult = query::entryPoint<QueryTypeOfSymbol>({ elem.symbol });
				if (qresult->hasValue()) {
					auto symbol_type = qresult->valueOrPanic();
					if (symbol_type.getType().getKind() == compiler::tsh::Kind::Meta) {
						out(position, StandardTokenType::Type);
						return;
					}
				}
				out(position, StandardTokenType::Variable);
				return;
			}
			default:
				out(position, StandardTokenType::Variable);
				return;
			}
		}

		void visitReusableExpr(const code::ReusableExpr& elem) override {
			static std::unordered_set<code::HOUTExprID> visited_exprs;
			if (visited_exprs.contains(elem.inner->getID())) return;
			visited_exprs.insert(elem.inner->getID());
			elem.inner->acceptVisitor(*this);
		}

		void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& elem) override {
			elem.lhs->acceptVisitor(*this);
			elem.rhs->acceptVisitor(*this);
		}

		void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& elem) override {
			elem.expr->acceptVisitor(*this);
		}

		void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& elem) override {
			elem.condition->acceptVisitor(*this);
			elem.if_true->acceptVisitor(*this);
			elem.if_false->acceptVisitor(*this);
		}

		void visitChainComparisonExpr(const code::ChainComparisonExpr& elem) override {
			for (const auto& comparison: elem.comparisons) comparison->acceptVisitor(*this);
		}

		void visitParenthesisExpr(const code::ParenthesisExpr& elem) override {
			elem.inner->acceptVisitor(*this);
		}

		void visitTupleExpr(const code::TupleExpr& elem) override {
			for (const auto& item: elem.elements) item->acceptVisitor(*this);
		}

		void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& elem) override {
			for (const auto& arg: elem.subtypes) arg->acceptVisitor(*this);
		}

		void visitCallExpr(const code::CallExpr& elem) override {
			elem.callee->acceptVisitor(*this);
			for (const auto& arg: elem.arguments) arg->acceptVisitor(*this);
		}

		void visitAccessExpr(const code::AccessExpr& elem) override {
			elem.base->acceptVisitor(*this);

			auto maybe_position = elem.origin.getSourcePosition();
			if_opt_none(maybe_position) return;
			out(maybe_position.value(), StandardTokenType::Property);
		}

		void visitIndexExpr(const code::IndexExpr& elem) override {
			elem.base->acceptVisitor(*this);
			elem.index->acceptVisitor(*this);
		}

		void visitSequenceExpr(const code::SequenceExpr& elem) override {
			for (const auto& item: elem.expressions) item->acceptVisitor(*this);
		}

		void visitBoxOfExpr(const code::BoxOfExpr& elem) override {
			elem.inner->acceptVisitor(*this);
		}

		void visitRefOfExpr(const code::RefOfExpr& elem) override {
			elem.inner->acceptVisitor(*this);
		}

		void visitDerefExpr(const code::DerefExpr& elem) override {
			elem.inner->acceptVisitor(*this);
		}

		void visitCastExpr(const code::CastExpr& elem) override {
			elem.source_expr->acceptVisitor(*this);
		}

		void visitLiftToTypeExpr(const code::LiftToTypeExpr& elem) override {
			elem.value_expr->acceptVisitor(*this);
		}

		void visitListPushExpr(const code::ListPushExpr& elem) override {
			elem.list->acceptVisitor(*this);
			elem.element->acceptVisitor(*this);
		}

		void visitListPopExpr(const code::ListPopExpr& elem) override {
			elem.list->acceptVisitor(*this);
			elem.count->acceptVisitor(*this);
		}
	};

	base::Optional<TokenContext> precalculateTokensForNode(pst::Access<pst::LangElement> elem) {
		TokenContext result;

		if (TokenTopLevelVisitor::supports(elem))
			TokenTopLevelVisitor::precalculateTokens(elem, result);
		else if (TokenHoutExprVisitor::supports(elem))
			TokenHoutExprVisitor::precalculateTokens(elem, result);
		else
			return {};

		// I am not sure if having this sort has 0-impact when the queue is empty,
		// and it is empty in 90% of cases.
		std::ranges::sort(
			result.precalculated, std::ranges::less{}, &PrecalculatedSemanticToken::position
		);
		return result;
	}

	void getSemanticTokens(
		pst::AccessLocked<pst::LangElement> element_locked,
		std::vector<SemanticToken>&         result,
		TokenContext&                       token_context
	) {
		base::Optional<TokenContext> new_local_context;

		if (not element_locked.illegalAccess().has_value()) return;
		auto element = element_locked.illegalAccess().value();

		if (token_context.precalculate_for_children)
			new_local_context = precalculateTokensForNode(element);

		auto& local_context
			= (new_local_context.has_value()) ? new_local_context.value() : token_context;

		for (auto sub: element->viewSubElements()) {
			variant_match(sub) {
				variant_case(pst::LangElement::SubToken, token) {
					auto& precalculated_candidate = local_context.precalculated.front();
					// add token to list
					if (precalculated_candidate.isPrecalculatedFor(token)) {
						result.emplace_back(token, precalculated_candidate.correct_type);
						local_context.precalculated.pop_front();
					} else if (local_context.default_identifier_type.has_value()
					           && token->getType() == lexer::Token::Type::Identifier) {
						result.emplace_back(token, local_context.default_identifier_type.value());
					} else {
						result.emplace_back(token);
					}
				}
				variant_case(pst::LangElement::Child, child) {
					// recursive token generation
					getSemanticTokens(child, result, local_context);
				}
				variant_case(pst::LangElement::NamedChild, child) {
					getSemanticTokens(child.element, result, local_context);
				}
			}
		}
	}

	std::string getSemanticTokens(base::Ref<compiler::frontend::SourceFile> file) {
		return getSemanticTokens(std::vector{ file });
	}

	std::string getSemanticTokens(const std::vector<base::Ref<compiler::frontend::SourceFile>>& files
	) {
		std::vector<SemanticToken> tokens;
		for (auto& file: files) {
			auto         pst     = file->getPST();
			auto         element = pst->getRootElement();
			TokenContext token_context;
			getSemanticTokens(element, tokens, token_context);
		}

		std::vector<std::string> token_strings(tokens.size());
		for (usize i = 0; i < token_strings.size(); i++) token_strings[i] = tokens[i].toJSON();

		return jsonList(token_strings);
	}
}
