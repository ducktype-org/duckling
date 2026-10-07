// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file semantic_tokens.cpp
 * @brief This file defines the SemanticToken class, which provides functionality to export
 * semantic tokens in JSON format.
 */
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/selector_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/ls_utils/ls_utils.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <lsp_interface/semantic_tokens.hpp>
#include <lsp_interface/utils.hpp>

#include <base/extend_cpp/stringifyable_enum.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>
#include <lexer/token.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace lsp {
	namespace {
		base::Optional<dia::SourcePosition> getOriginPosition(
			const compiler::helios::code::ElementOrigin& origin
		) {
			return origin.getStablePosition().map([](const dia::StablePosition& stable_pos) {
				return stable_pos.getActiveSourcePositionIllegalAccess();
			});
		}
	}

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
		case lTT::FormatString:
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
		return std::format(
			R"({{"line":{},"startCharacter":{},"length":{},"tokenType":{},"tokenModifiers":0}})",
			line,
			start_character,
			length,
			static_cast<int8_t>(type)
		);
	}

	/**
	 * @brief Represents a token we have identified the semantic meaning of
	 * before we put it in the final result.
	 */
	struct PrecalculatedSemanticToken {
		dia::SourcePosition position;
		StandardTokenType   correct_type;

		/**
		 * @brief Checks if the precalculated token corresponds to the given token based on its
		 * position.
		 */
		[[nodiscard]] bool isPrecalculatedFor(const CRef<lexer::Token>& token) const {
			// @TODO: #2301 Maybe in the futurue fix this to compare against whole position, not
			// just end.
			return token->getPosition().getEnd() == position.getEnd();
		}
	};

	/**
	 * @brief This is the context needed for the PST subtree evaluation.
	 */
	struct TokenContext {
		/// List of tokens we have precalculated so far in the good order.
		std::vector<PrecalculatedSemanticToken> precalculated{};

		/// Some PST subtrees might want to change the default identifier token type for their children.
		base::Optional<StandardTokenType> default_identifier_type{};

		/// Whether we should invoke the semantic analysis for children of the node.
		/// For example, we only analyze the top-level expression and do not call the semantic
		/// analysis for the sub-expressions.
		bool precalculate_for_children = true;

		/**
		 * @brief Get the closest precalculated token (first in the left-to-right order).
		 */
		PrecalculatedSemanticToken& getFirstPrecalculated() { return precalculated.back(); }

		/**
		 * @brief Remove the first precalculated token from the list, after we have used it.
		 */
		void advancePrecalculated() { precalculated.pop_back(); }
	};

	struct TokenTopLevelVisitor final: public pst::PstVisitorEmpty {
		static bool supports(pst::Access<pst::LangElement> element) {
			// For now we only support precalculation for function declarations, but this can be
			// extended in the future.
			switch (element->getElementKind()) {
			case pst::ElementKind::Import:
			case pst::ElementKind::Using:
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

		explicit TokenTopLevelVisitor(std::vector<PrecalculatedSemanticToken>& identified_tokens):
			  identified_tokens(identified_tokens) {}

		std::vector<PrecalculatedSemanticToken>& identified_tokens;


		~TokenTopLevelVisitor() override = default;

		void out(dia::SourcePosition position, StandardTokenType correct_type) {
			identified_tokens.push_back({ position, correct_type });
		}

		/**
		 * @brief Marks the dotted prefixes of @p list, nested lists included, as namespaces.
		 */
		template<typename ListType>
		void outSelectorNames(pst::AccessLocked<ListType> list_locked) {
			if_opt_none(list_locked.illegalAccess()) return;
			for (const auto& selector_locked: *list_locked.illegalAccess().value()) {
				if_opt_none(selector_locked.illegalAccess()) continue;
				auto selector = selector_locked.illegalAccess().value();
				for (usize idx = 0; idx < selector->numberOfNames(); idx++) {
					out(selector->getNameByIndex(idx)
					        .illegalAccess()
					        .value()
					        ->getSourcePosition()
					        .illegalAccess(),
					    StandardTokenType::Namespace);
				}
				if (auto nested = selector->getNested()) outSelectorNames(nested.value());
			}
		}

		void visitImport(pst::Access<pst::Import> elem) override {
			outSelectorNames(elem->getSelectors());
		}

		void visitUsing(pst::Access<pst::Using>) override {
			// For now empty
		}

		void visitNamespace(pst::Access<pst::Namespace> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Namespace);
		}

		void visitClass(pst::Access<pst::Class> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Class);
		}

		void visitFun(pst::Access<pst::Fun> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Function);
		}

		void visitFunDecl(pst::Access<pst::FunDecl> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Function);
		}

		void visitVariable(pst::Access<pst::Variable> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Variable);
		}

		void visitMethod(pst::Access<pst::Method> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Method);
		}

		void visitField(pst::Access<pst::Field> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Property);
		}

		void visitParam(pst::Access<pst::Param> elem) override {
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    StandardTokenType::Parameter);
		}

		void visitCallArgument(pst::Access<pst::CallArgument> elem) override {
			if_opt_some(elem->getArgName(), _) {
				out(elem->getArgName()->illegalAccess().value()->getSourcePosition().illegalAccess(),
				    StandardTokenType::Parameter);
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
			out(elem->getName().illegalAccess().value()->getSourcePosition().illegalAccess(),
			    token_type);
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

			auto hout_expr_result = std::any_cast<CRef<query::QResult<Box<code::Expr>>>>(
				query::utils::withContextCompute([&](query::Context& ctx) {
					return compiler::helios::ls::getHoutExpr(ctx, expr_element);
				})
			);
			if (hout_expr_result->hasFailed()) return;

			auto&                hout_expr = hout_expr_result->valueOrPanic();
			TokenHoutExprVisitor visitor(result.precalculated);
			hout_expr->acceptVisitor(visitor);
			result.default_identifier_type.emplace(StandardTokenType::Namespace);

			if_opt_some(getOriginPosition(hout_expr->origin), whole_expr_pos) {
				// Filter the results.pre-calculated to only keep the tokens that are inside the
				// expression position. This is needed because things like "default parameter value"
				// are in the HOUT in the call, but their source position is in the function declaration.
				auto is_inside_expr = [&](const PrecalculatedSemanticToken& token) {
					return token.position.getStart() >= whole_expr_pos.getStart()
					    && token.position.getEnd() <= whole_expr_pos.getEnd();
				};
				std::erase_if(result.precalculated, [&](const PrecalculatedSemanticToken& token) {
					return !is_inside_expr(token);
				});
			}
		}

		explicit TokenHoutExprVisitor(std::vector<PrecalculatedSemanticToken>& identified_tokens):
			  identified_tokens(identified_tokens) {}

		std::vector<PrecalculatedSemanticToken>& identified_tokens;
		std::unordered_set<code::HOUTExprID>     visited_reusable_exprs;

		~TokenHoutExprVisitor() override = default;

		void out(dia::SourcePosition pos, StandardTokenType correct_type) {
			identified_tokens.push_back({ pos, correct_type });
		}

		void out(CRef<lexer::Token> token, StandardTokenType correct_type) {
			identified_tokens.push_back({ token->getPosition(), correct_type });
		}

		void visitIdentifierExpr(const code::IdentifierExpr& elem) override {
			auto maybe_position = getOriginPosition(elem.origin);
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
			if (visited_reusable_exprs.contains(elem.inner->getID())) return;
			visited_reusable_exprs.insert(elem.inner->getID());
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

			auto maybe_position = getOriginPosition(elem.origin);
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

		void visitRefOfExpr(const code::RefOfExpr& elem) override {
			elem.inner->acceptVisitor(*this);
		}

		void visitPtrOfExpr(const code::PtrOfExpr& elem) override {
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
	};

	/**
	 * @brief Do a precalculation pass for the given node.
	 * @return A new context if some visitor accepted the node and did the precalculation,
	 * or empty optional if no visitor supported the node.
	 */
	base::Optional<TokenContext> precalculateTokensForNode(pst::Access<pst::LangElement> elem) {
		TokenContext result;

		if (TokenTopLevelVisitor::supports(elem))
			TokenTopLevelVisitor::precalculateTokens(elem, result);
		else if (TokenHoutExprVisitor::supports(elem))
			TokenHoutExprVisitor::precalculateTokens(elem, result);
		else
			return {};

		// We have to sort the precalculated tokens, because elements like for example
		// methods have their first argument before the method name, meaning we can't ensure
		// the right order without sorting. But the sorting is not expensive
		// because the number of precalculated tokens is very small.
		std::ranges::sort(
			result.precalculated,
			[](const PrecalculatedSemanticToken& a, const PrecalculatedSemanticToken& b) {
				return a.position.getEnd() > b.position.getEnd();  // bigger first, smaller last
			}
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
					if (not local_context.precalculated.empty()) {
						auto& precalculated_candidate = local_context.getFirstPrecalculated();
						// add token to list
						if (precalculated_candidate.isPrecalculatedFor(token)) {
							result.emplace_back(token, precalculated_candidate.correct_type);
							local_context.advancePrecalculated();
							continue;
						}
					}
					if (local_context.default_identifier_type.has_value()
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
