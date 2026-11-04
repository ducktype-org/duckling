// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/**
 * @file semantic_tokens.cpp
 * @brief Classifies the tokens of a source file, refining the lexical kind of the names with the
 * declarations and the HOUT of the expressions they appear in.
 */
#include <frontend/pst_parser/access.hpp>
#include <frontend/pst_parser/element_kind.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/all_class_elements.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/lists/selector_list.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/ls_utils/ls_utils.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <lsp_interface/semantic_tokens.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/source_position.hpp>
#include <lexer/token.hpp>
#include <query_framework/context/context.hpp>

#include <algorithm>
#include <unordered_set>

namespace duck_ls {
	namespace {
		using TokenType = lsp::SemanticTokenTypes;

		base::Optional<dia::SourcePosition> getOriginPosition(
			const compiler::helios::code::ElementOrigin& origin
		) {
			return origin.getStablePosition().map([](const dia::StablePosition& stable_pos) {
				return stable_pos.getActiveSourcePositionIllegalAccess();
			});
		}

		/**
		 * @brief The type of a token when nothing but its lexical kind is known, or empty when
		 * the token is not highlighted at all.
		 */
		base::Optional<TokenType> translateType(lexer::Token::Type type) {
			using lTT = lexer::Token::Type;
			switch (type) {
			case lTT::Keyword:
				return TokenType::Keyword;
			case lTT::NumLiteral:
			case lTT::NumLiteralGroup:
				return TokenType::Number;
			case lTT::String:
			case lTT::Char:
			case lTT::FormatString:
				return TokenType::String;
			case lTT::Operator:
			case lTT::Sentinel:
				return TokenType::Operator;
			case lTT::Comment:
				return TokenType::Comment;
			case lTT::TypeSpecifier:
				return TokenType::Type;
			case lTT::Identifier:
				// Further classification would require semantic analysis.
				return TokenType::Variable;
			default:
				return {};
			}
		}

		/**
		 * @brief The type of a constant: a constant holding a type is highlighted as one.
		 */
		TokenType constType(query::Context& ctx, compiler::helios::SymID symbol) {
			auto qresult = ctx.query<compiler::helios::QueryTypeOfSymbol>({ symbol });
			if (qresult->hasFailed()) return TokenType::Variable;
			if (qresult->valueOrPanic().getType().getKind() == compiler::tsh::Kind::Meta)
				return TokenType::Type;
			return TokenType::Variable;
		}

		struct ClassifiedToken {
			dia::SourcePosition position;
			TokenType           type;
		};

		/**
		 * @brief Represents a token we have identified the semantic meaning of
		 * before we put it in the final result.
		 */
		struct PrecalculatedSemanticToken {
			dia::SourcePosition position;
			TokenType           correct_type;

			/**
			 * @brief Checks if the precalculated token corresponds to the given token based on its
			 * position.
			 */
			[[nodiscard]] bool isPrecalculatedFor(const CRef<lexer::Token>& token) const {
				// @TODO: #2301 Maybe in the futurue fix this to compare against whole position,
				// not just end.
				return token->getPosition().getEnd() == position.getEnd();
			}
		};

		/**
		 * @brief This is the context needed for the PST subtree evaluation.
		 */
		struct TokenContext {
			/// List of tokens we have precalculated so far in the good order.
			std::vector<PrecalculatedSemanticToken> precalculated{};

			/// Some PST subtrees might want to change the default identifier token type for their
			/// children.
			base::Optional<TokenType> default_identifier_type{};

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

			static void precalculateTokens(
				query::Context& ctx, pst::Access<pst::LangElement> element, TokenContext& result
			) {
				TokenTopLevelVisitor visitor(ctx, result.precalculated);
				element->acceptVisitor(visitor);
			}

			TokenTopLevelVisitor(
				query::Context& ctx, std::vector<PrecalculatedSemanticToken>& identified_tokens
			):
				  ctx(ctx),
				  identified_tokens(identified_tokens) {}

			query::Context&                          ctx;
			std::vector<PrecalculatedSemanticToken>& identified_tokens;

			~TokenTopLevelVisitor() override = default;

			void out(dia::SourcePosition position, TokenType correct_type) {
				identified_tokens.push_back({ position, correct_type });
			}

			template<typename Name>
			void outName(const Name& name, TokenType correct_type) {
				out(name.illegalAccess().value()->getSourcePosition().illegalAccess(), correct_type);
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
					for (usize idx = 0; idx < selector->numberOfNames(); idx++)
						outName(selector->getNameByIndex(idx), TokenType::Namespace);
					if (auto nested = selector->getNested()) outSelectorNames(nested.value());
				}
			}

			void visitImport(pst::Access<pst::Import> elem) override {
				outSelectorNames(elem->getSelectors());
			}

			void visitUsing(pst::Access<pst::Using>) override {}

			void visitNamespace(pst::Access<pst::Namespace> elem) override {
				outName(elem->getName(), TokenType::Namespace);
			}

			void visitClass(pst::Access<pst::Class> elem) override {
				outName(elem->getName(), TokenType::Class);
			}

			void visitFun(pst::Access<pst::Fun> elem) override {
				outName(elem->getName(), TokenType::Function);
			}

			void visitFunDecl(pst::Access<pst::FunDecl> elem) override {
				outName(elem->getName(), TokenType::Function);
			}

			void visitVariable(pst::Access<pst::Variable> elem) override {
				outName(elem->getName(), TokenType::Variable);
			}

			void visitMethod(pst::Access<pst::Method> elem) override {
				outName(elem->getName(), TokenType::Method);
			}

			void visitField(pst::Access<pst::Field> elem) override {
				outName(elem->getName(), TokenType::Property);
			}

			void visitParam(pst::Access<pst::Param> elem) override {
				outName(elem->getName(), TokenType::Parameter);
			}

			void visitCallArgument(pst::Access<pst::CallArgument> elem) override {
				if_opt_some(elem->getArgName(), arg_name) outName(arg_name, TokenType::Parameter);
			}

			void visitConst(pst::Access<pst::Const> elem) override {
				TokenType token_type = TokenType::Variable;
				auto      symbol     = compiler::helios::ls::getSymbolOfStmt(ctx, elem);
				if (symbol.hasValue()) token_type = constType(ctx, symbol.valueOrPanic());
				outName(elem->getName(), token_type);
			}
		};

		using namespace compiler::helios;

		class TokenHoutExprVisitor final: public code::HoutExprVisitorEmpty {
		public:
			static bool supports(pst::Access<pst::LangElement> element) {
				return element.dynamicCast<pst::ExprElement>().has_value()
				    && element.dynamicCast<pst::expr::Assignment>().empty();
			}

			static void precalculateTokens(
				query::Context& ctx, pst::Access<pst::LangElement> element, TokenContext& result
			) {
				auto expr_element = element.dynamicCast<pst::ExprElement>().value();

				result.precalculate_for_children = false;

				auto hout_expr_result = compiler::helios::ls::getHoutExpr(ctx, expr_element);
				if (hout_expr_result->hasFailed()) return;

				auto&                hout_expr = hout_expr_result->valueOrPanic();
				TokenHoutExprVisitor visitor(ctx, result.precalculated);
				hout_expr->acceptVisitor(visitor);
				result.default_identifier_type.emplace(TokenType::Namespace);

				if_opt_some(getOriginPosition(hout_expr->origin), whole_expr_pos) {
					// Filter the results.pre-calculated to only keep the tokens that are inside the
					// expression position. This is needed because things like "default parameter
					// value" are in the HOUT in the call, but their source position is in the
					// function declaration.
					std::erase_if(result.precalculated, [&](const PrecalculatedSemanticToken& token) {
						return token.position.getStart() < whole_expr_pos.getStart()
						    || token.position.getEnd() > whole_expr_pos.getEnd();
					});
				}
			}

			TokenHoutExprVisitor(
				query::Context& ctx, std::vector<PrecalculatedSemanticToken>& identified_tokens
			):
				  ctx(ctx),
				  identified_tokens(identified_tokens) {}

			query::Context&                          ctx;
			std::vector<PrecalculatedSemanticToken>& identified_tokens;
			std::unordered_set<code::HOUTExprID>     visited_reusable_exprs;

			~TokenHoutExprVisitor() override = default;

			void out(const code::ElementOrigin& origin, TokenType correct_type) {
				if_opt_some(getOriginPosition(origin), position)
					identified_tokens.push_back({ position, correct_type });
			}

			void visitIdentifierExpr(const code::IdentifierExpr& elem) override {
				switch (kind(elem.symbol)) {
				case SymbolKind::Namespace:
					out(elem.origin, TokenType::Namespace);
					return;
				case SymbolKind::Function:
				case SymbolKind::FunctionDeclaration:
					out(elem.origin, TokenType::Function);
					return;
				case SymbolKind::Method:
					out(elem.origin, TokenType::Method);
					return;
				case SymbolKind::Parameter:
					out(elem.origin, TokenType::Parameter);
					return;
				case SymbolKind::Class:
					out(elem.origin, TokenType::Class);
					return;
				case SymbolKind::Field:
					out(elem.origin, TokenType::Property);
					return;
				case SymbolKind::Const:
					out(elem.origin, constType(ctx, elem.symbol));
					return;
				default:
					out(elem.origin, TokenType::Variable);
					return;
				}
			}

			void visitReusableExpr(const code::ReusableExpr& elem) override {
				if (!visited_reusable_exprs.insert(elem.inner->getID()).second) return;
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

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& elem
			) override {
				for (const auto& arg: elem.subtypes) arg->acceptVisitor(*this);
			}

			void visitOptionalTypeConstructorExpr(const code::OptionalTypeConstructorExpr& elem
			) override {
				elem.value_type->acceptVisitor(*this);
			}

			void visitCallExpr(const code::CallExpr& elem) override {
				elem.callee->acceptVisitor(*this);
				for (const auto& arg: elem.arguments) arg->acceptVisitor(*this);
			}

			void visitAccessExpr(const code::AccessExpr& elem) override {
				elem.base->acceptVisitor(*this);
				out(elem.origin, TokenType::Property);
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
		base::Optional<TokenContext> precalculateTokensForNode(
			query::Context& ctx, pst::Access<pst::LangElement> elem
		) {
			TokenContext result;

			if (TokenTopLevelVisitor::supports(elem))
				TokenTopLevelVisitor::precalculateTokens(ctx, elem, result);
			else if (TokenHoutExprVisitor::supports(elem))
				TokenHoutExprVisitor::precalculateTokens(ctx, elem, result);
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

		void classifyTokens(
			query::Context&                     ctx,
			pst::AccessLocked<pst::LangElement> element_locked,
			std::vector<ClassifiedToken>&       result,
			TokenContext&                       token_context
		) {
			if_opt_none(element_locked.illegalAccess()) return;
			auto element = element_locked.illegalAccess().value();

			base::Optional<TokenContext> new_local_context;
			if (token_context.precalculate_for_children)
				new_local_context = precalculateTokensForNode(ctx, element);

			auto& local_context
				= (new_local_context.has_value()) ? new_local_context.value() : token_context;

			auto emit = [&](const CRef<lexer::Token>& token, base::Optional<TokenType> type) {
				if_opt_some(type, known) result.push_back({ token->getPosition(), known });
			};

			for (auto sub: element->viewSubElements()) {
				variant_match(sub) {
					variant_case(pst::LangElement::SubToken, token) {
						if (not local_context.precalculated.empty()) {
							auto& precalculated_candidate = local_context.getFirstPrecalculated();
							if (precalculated_candidate.isPrecalculatedFor(token)) {
								emit(token, precalculated_candidate.correct_type);
								local_context.advancePrecalculated();
								continue;
							}
						}
						if (local_context.default_identifier_type.has_value()
						    && token->getType() == lexer::Token::Type::Identifier) {
							emit(token, local_context.default_identifier_type);
						} else {
							emit(token, translateType(token->getType()));
						}
					}
					variant_case(pst::LangElement::Child, child) {
						classifyTokens(ctx, child, result, local_context);
					}
					variant_case(pst::LangElement::NamedChild, child) {
						classifyTokens(ctx, child.element, result, local_context);
					}
				}
			}
		}

		/**
		 * @brief Classifies the lexer tokens by their lexical kind only, descending into bracket
		 * groups.
		 */
		void classifyLexerTokens(const lexer::Tokens& tokens, std::vector<ClassifiedToken>& result) {
			for (const auto& token: tokens) {
				if (token.getType() == lexer::Token::Type::BracketGroup) {
					classifyLexerTokens({ token.getSentinelBegin() }, result);
					classifyLexerTokens(token.getRecursive(), result);
					classifyLexerTokens({ token.getSentinelEnd() }, result);
					continue;
				}
				if_opt_some(translateType(token.getType()), type)
					result.push_back({ token.getPosition(), type });
			}
		}

		/**
		 * @brief Encodes the tokens relative to each other, dropping the ones overlapping a token
		 * before them.
		 */
		lsp::SemanticTokens encode(std::vector<ClassifiedToken>& tokens) {
			std::ranges::stable_sort(tokens, {}, [](const ClassifiedToken& token) {
				return token.position.getStart();
			});

			lsp::SemanticTokens result;
			lsp::Uint           previous_line      = 0;
			lsp::Uint           previous_character = 0;
			usize               covered_until      = 0;

			for (const auto& token: tokens) {
				const usize start = token.position.getStart();
				const usize end   = token.position.getEnd() + 1;
				if (start < covered_until) continue;
				covered_until = end;

				auto [line, column]      = token.position.getStartLineColumn();
				const auto lsp_line      = static_cast<lsp::Uint>(line - 1);
				const auto lsp_character = static_cast<lsp::Uint>(column - 1);
				const auto delta_line    = lsp_line - previous_line;

				result.data.push_back(delta_line);
				result.data.push_back(
					delta_line == 0 ? lsp_character - previous_character : lsp_character
				);
				result.data.push_back(static_cast<lsp::Uint>(end - start));
				result.data.push_back(static_cast<lsp::Uint>(token.type));
				result.data.push_back(0);

				previous_line      = lsp_line;
				previous_character = lsp_character;
			}
			return result;
		}
	}

	lsp::SemanticTokensLegend semanticTokensLegend() {
		lsp::SemanticTokensLegend legend;
		for (usize i = 0; i < static_cast<usize>(TokenType::MAX_VALUE); i++)
			legend.tokenTypes.push_back(lsp::SemanticTokenTypesEnum(static_cast<TokenType>(i)));
		return legend;
	}

	lsp::SemanticTokens collectSemanticTokens(
		query::Context& ctx, base::Ref<compiler::frontend::SourceFile> file, bool analyze
	) {
		std::vector<ClassifiedToken> tokens;
		if (analyze) {
			TokenContext token_context;
			classifyTokens(ctx, file->getPST()->getRootElement(), tokens, token_context);
		} else {
			classifyLexerTokens(file->getPST()->getFile()->getTokenData().tokens, tokens);
		}
		return encode(tokens);
	}
}
