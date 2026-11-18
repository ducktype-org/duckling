// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/source_file.hpp>
#include <frontend/pst_parser/access/access_pst.hpp>
#include <frontend/pst_parser/base_element/lang_parser_element.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/meta.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/expr_element.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/wrapper_elements/identifier_wrapper.hpp>
#include <frontend/pst_parser/pst/parsed_pst.hpp>
#include <helios/hout/elements/expr.hpp>
#include <helios/hout/origin.hpp>
#include <helios/hout/visitors.hpp>
#include <helios/ls_utils/ls_utils.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <lsp_interface/hover.hpp>

#include <diagnostic/source_position.hpp>
#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace duck_ls {

	namespace {

		using namespace compiler::helios;

		/**
		 * @brief Whether `position`, the range a node takes in the file, covers `offset`. The end
		 * of a source position is the offset of its last character.
		 */
		bool covers(const dia::SourcePosition& position, usize offset) {
			return position.getStart() <= offset && offset <= position.getEnd();
		}

		/**
		 * @brief The two places under the cursor that a hover can describe, found on the way
		 * from the root of the PST down to the offset.
		 */
		struct PstPath final {
			/// The outermost expression containing the offset. HELIOS gives HOUT only for whole
			/// expressions like this one, not for their parts, so the hover asks for this one and
			/// then looks inside it. The two sides of an assignment count as separate expressions.
			base::Optional<pst::Access<pst::ExprElement>> top_expression{};
			/// The innermost declaration whose name contains the offset, like `var sum` when the
			/// cursor is on `sum`.
			base::Optional<pst::Access<pst::Stmt>> declaration{};
		};

		/**
		 * @brief Whether `statement` declares a symbol whose name covers `offset`.
		 */
		bool declaresNameAt(pst::Access<pst::Stmt> statement, usize offset) {
			if (statement->isDeclaration() != pst::DeclKind::Symbol) return false;

			auto identifier_locked = statement->getDeclSymbolIdentifier();
			if (!identifier_locked.has_value()) return false;

			auto identifier = identifier_locked.value().illegalAccess();
			if (!identifier.has_value()) return false;

			return covers(identifier.value()->getSourcePosition().illegalAccess(), offset);
		}

		/**
		 * @brief Walks from `element` down to the smallest element containing `offset`, filling
		 * `path` on the way.
		 *
		 * At each level it goes into the one child that contains `offset`, and stops when no
		 * child does. On the way it records the first expression it passes as
		 * `path.top_expression` and the last declaration whose name contains `offset` as
		 * `path.declaration`.
		 */
		void descend(pst::Access<pst::LangElement> element, usize offset, PstPath& path) {
			if (path.top_expression.empty()
			    && element.dynamicCast<pst::expr::Assignment>().empty()) {
				if (auto expression = element.dynamicCast<pst::ExprElement>())
					path.top_expression.emplace(expression.value());
			}

			if (auto statement = element.dynamicCast<pst::Stmt>()) {
				if (declaresNameAt(statement.value(), offset))
					path.declaration.emplace(statement.value());
			}

			for (auto child_locked: element->viewChildren()) {
				auto child = child_locked.illegalAccess();
				if (!child.has_value()) continue;
				if (!covers(child.value()->getSourcePosition().illegalAccess(), offset)) continue;

				descend(child.value(), offset, path);
				return;
			}
		}

		/**
		 * @brief Finds the smallest HOUT expression under the cursor.
		 *
		 * Visits every expression in the HOUT of `PstPath::top_expression` and keeps the
		 * smallest one whose source position contains the offset.
		 *
		 * HELIOS also adds expressions that are not written in the code, such as an implicit cast
		 * or dereference, or a default argument copied into a call. Two rules keep them out:
		 * - An added expression is never chosen itself, but the visit goes on into its children,
		 *   so hovering over `y` in an implicit cast of `y` shows `y`, not the cast.
		 * - An expression counts only if its position is in the same file and inside the range of
		 *   the top-level expression. A copied default argument keeps the position of the
		 *   declaration it came from, which can be anywhere.
		 */
		class InnermostExpressionFinder final: public code::HoutExprVisitor {
		public:
			InnermostExpressionFinder(
				query::Context& context, usize cursor_offset, dia::SourcePosition top_bounds
			):
				  ctx(context),
				  offset(cursor_offset),
				  bounds(top_bounds) {}

			/// The best candidate so far, if any.
			const code::Expr* found = nullptr;

			// One method per kind of HOUT expression. `expr->acceptVisitor(*this)` calls the one
			// matching the kind of `expr`, so no chain of casts is needed. Each method considers
			// its own expression and then visits its subexpressions, parents before children.
			// They all look alike; what differs is which subexpressions each kind has.

			void visitLiteralUnitExpr(const code::LiteralUnitExpr& expr) override {
				consider(expr);
			}

			void visitLiteralNumericExpr(const code::LiteralNumericExpr& expr) override {
				consider(expr);
			}

			void visitLiteralBoolExpr(const code::LiteralBoolExpr& expr) override {
				consider(expr);
			}

			void visitLiteralCharExpr(const code::LiteralCharExpr& expr) override {
				consider(expr);
			}

			void visitLiteralStringExpr(const code::LiteralStringExpr& expr) override {
				consider(expr);
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) override {
				consider(expr);
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) override { consider(expr); }

			void visitReusableExpr(const code::ReusableExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitBinaryOperatorExpr(const code::BinaryOperatorExpr& expr) override {
				consider(expr);
				expr.lhs->acceptVisitor(*this);
				expr.rhs->acceptVisitor(*this);
			}

			void visitUnaryOperatorExpr(const code::UnaryOperatorExpr& expr) override {
				consider(expr);
				expr.expr->acceptVisitor(*this);
			}

			void visitTernaryOperatorExpr(const code::TernaryOperatorExpr& expr) override {
				consider(expr);
				expr.condition->acceptVisitor(*this);
				expr.if_true->acceptVisitor(*this);
				expr.if_false->acceptVisitor(*this);
			}

			void visitChainComparisonExpr(const code::ChainComparisonExpr& expr) override {
				consider(expr);
				for (const auto& comparison: expr.comparisons) comparison->acceptVisitor(*this);
			}

			void visitTupleExpr(const code::TupleExpr& expr) override {
				consider(expr);
				for (const auto& element: expr.elements) element->acceptVisitor(*this);
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) override {
				consider(expr);
				for (const auto& subtype: expr.subtypes) subtype->acceptVisitor(*this);
			}

			void visitFunctionTypeExpr(const code::FunctionTypeExpr& expr) override {
				consider(expr);
				for (const auto& parameter: expr.parameters) parameter->acceptVisitor(*this);
				expr.result->acceptVisitor(*this);
			}

			void visitOptionalTypeConstructorExpr(const code::OptionalTypeConstructorExpr& expr
			) override {
				consider(expr);
				expr.value_type->acceptVisitor(*this);
			}

			void visitVariantConstructExpr(const code::VariantConstructExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitMatchExpr(const code::MatchExpr& expr) override {
				consider(expr);
				expr.subject->acceptVisitor(*this);
				for (const auto& match_case: expr.cases) match_case.result->acceptVisitor(*this);
			}

			void visitCallExpr(const code::CallExpr& expr) override {
				consider(expr);
				expr.callee->acceptVisitor(*this);
				for (const auto& argument: expr.arguments) argument->acceptVisitor(*this);
			}

			void visitAccessExpr(const code::AccessExpr& expr) override {
				consider(expr);
				expr.base->acceptVisitor(*this);
			}

			void visitIndexExpr(const code::IndexExpr& expr) override {
				consider(expr);
				expr.base->acceptVisitor(*this);
				expr.index->acceptVisitor(*this);
			}

			void visitSequenceExpr(const code::SequenceExpr& expr) override {
				consider(expr);
				for (const auto& item: expr.expressions) item->acceptVisitor(*this);
			}

			void visitMoveExpr(const code::MoveExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitRefOfExpr(const code::RefOfExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitPtrOfExpr(const code::PtrOfExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitDerefExpr(const code::DerefExpr& expr) override {
				consider(expr);
				expr.inner->acceptVisitor(*this);
			}

			void visitDefaultValueExpr(const code::DefaultValueExpr& expr) override {
				consider(expr);
			}

			void visitCreateAggregateExpr(const code::CreateAggregateExpr& expr) override {
				consider(expr);
				for (const auto& value: expr.values) value->acceptVisitor(*this);
			}

			void visitCastExpr(const code::CastExpr& expr) override {
				consider(expr);
				expr.source_expr->acceptVisitor(*this);
			}

			void visitLiftToTypeExpr(const code::LiftToTypeExpr& expr) override {
				consider(expr);
				expr.value_expr->acceptVisitor(*this);
			}

			// The statements inside a block are not visited: the language has no syntax for block
			// expressions yet (#3292), so a hover can only reach the blocks HELIOS creates itself.
			void visitBlockExpr(const code::BlockExpr& expr) override { consider(expr); }

		private:
			/// Used to ask HELIOS where each expression comes from in the source.
			query::Context& ctx;
			/// The cursor, in code points from the start of the file.
			usize offset;
			/// The range of `PstPath::top_expression` in the file. An expression outside it, or
			/// from another file, is never chosen.
			dia::SourcePosition bounds;
			/// How long `found` is (end minus start), kept so that each new candidate can be
			/// compared with it without asking HELIOS for its position again.
			usize found_size = 0;

			void consider(const code::Expr& expr) {
				if (expr.origin.isGenerated()) return;

				auto position_opt = expr.origin.getSourcePosition(ctx);
				if (!position_opt.has_value()) return;
				const auto& position = position_opt.value();

				if (position.getSource().get() != bounds.getSource().get()) return;
				if (position.getStart() < bounds.getStart() || position.getEnd() > bounds.getEnd())
					return;
				if (!covers(position, offset)) return;

				// Children are visited after their parents, so on a tie the deeper one wins.
				const usize size = position.getEnd() - position.getStart();
				if (found == nullptr || size <= found_size) {
					found      = &expr;
					found_size = size;
				}
			}
		};

		/**
		 * @brief Wraps `code` in a Markdown code block marked as Duckling, so the editor
		 * highlights it accordingly.
		 */
		std::string markdownCodeBlock(std::string_view code) {
			return std::string("```duckling\n").append(code).append("\n```");
		}

		/**
		 * @brief The type of `symbol` as text, or `fallback` when HELIOS does not know it.
		 */
		base::Optional<std::string> typeOfSymbol(
			query::Context& ctx, SymID symbol, base::Optional<std::string> fallback
		) {
			auto type_result = ctx.query<QueryTypeOfSymbol>({ symbol });
			if (type_result->hasValue()) return type_result->valueOrPanic().toString();
			return fallback;
		}

		/**
		 * @brief `<name>: <type>` for a symbol, or just `<name>` when its type is not known.
		 */
		std::string describeSymbol(
			query::Context& ctx, SymID symbol, base::Optional<std::string> fallback_type
		) {
			std::string line{ name(symbol).strView() };

			if (auto type = typeOfSymbol(ctx, symbol, std::move(fallback_type)))
				line.append(": ").append(type.value());

			return markdownCodeBlock(line);
		}

		/**
		 * @brief The hover text for an expression.
		 *
		 * If the expression is a name, like `sum`, or a field access, like `p.x`, the text is
		 * `<name>: <type>` of the variable, function or field it refers to. For any other
		 * expression, like `a + b`, the text is just the type of the expression.
		 */
		std::string describeExpression(query::Context& ctx, const code::Expr& expr) {
			std::string expression_type = expr.expression_type.getSymbolType().toString();

			if (const auto* identifier = dynamic_cast<const code::IdentifierExpr*>(&expr))
				return describeSymbol(ctx, identifier->symbol, expression_type);
			if (const auto* access = dynamic_cast<const code::AccessExpr*>(&expr))
				return describeSymbol(ctx, access->field, expression_type);

			return markdownCodeBlock(expression_type);
		}

		/**
		 * @brief The hover for the smallest expression under the cursor inside `top_expression`.
		 */
		base::Optional<std::string> hoverForExpression(
			query::Context& ctx, pst::Access<pst::ExprElement> top_expression, usize offset
		) {
			auto hout_result = ls::getHoutExpr(ctx, top_expression);
			if (!hout_result->hasValue()) return {};
			const auto& hout = hout_result->valueOrPanic();

			InnermostExpressionFinder finder(
				ctx, offset, top_expression->getSourcePosition().illegalAccess()
			);
			hout->acceptVisitor(finder);
			if (finder.found == nullptr) return {};

			return describeExpression(ctx, *finder.found);
		}

		/**
		 * @brief The hover for the symbol that `declaration` declares.
		 */
		base::Optional<std::string> hoverForDeclaration(
			query::Context& ctx, pst::Access<pst::Stmt> declaration
		) {
			auto symbol = ls::getSymbolOfStmt(ctx, declaration);
			if (!symbol.hasValue()) return {};
			return describeSymbol(ctx, symbol.valueOrPanic(), {});
		}

	}

	base::Optional<std::string> hoverMarkdownAt(
		base::Ref<compiler::frontend::SourceFile> file, usize offset
	) {
		try {
			auto pst = file->getPST();
			// Like the diagnostics (see `isModuleTreeParsedSuccessfully` in compiler.cpp), we only
			// run HELIOS on a file that parsed without errors: it expects a complete parse tree.
			if (pst->hasErrors()) return {};

			auto root = pst->getRootElement().illegalAccess();
			if (!root.has_value()) return {};

			PstPath path;
			descend(root.value(), offset, path);

			base::Optional<std::string> markdown;
			query::utils::withContextDo([&](query::Context& ctx) {
				if (path.top_expression.has_value())
					markdown = hoverForExpression(ctx, path.top_expression.value(), offset);
				if (markdown.empty() && path.declaration.has_value())
					markdown = hoverForDeclaration(ctx, path.declaration.value());
			});
			return markdown;
		} catch (const std::exception& e) {
			std::cerr << "duck_ls: hover failed: " << e.what() << "\n";
			return {};
		}
	}

}
