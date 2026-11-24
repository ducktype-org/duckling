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
#include <helios/utils/hout_walker_generic.hpp>
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
		 * @brief The smallest HOUT expression in `root` whose source position covers `offset`,
		 * or empty when there is none.
		 *
		 * HELIOS also adds expressions that are not written in the code, such as an implicit cast
		 * or dereference, or a default argument copied into a call. Two rules keep them out:
		 * - An added expression is never chosen itself, but the walk still goes into its children,
		 *   so hovering over `y` in an implicit cast of `y` shows `y`, not the cast.
		 * - An expression counts only if its position is in the same file and inside `bounds`,
		 *   the range of `root`. A copied default argument keeps the position of the declaration
		 *   it came from, which can be anywhere.
		 */
		base::Optional<CRef<code::Expr>> findInnermostExpression(
			query::Context& ctx, const code::Expr& root, usize offset, const dia::SourcePosition& bounds
		) {
			base::Optional<CRef<code::Expr>> found{};
			// How long `found` is (end minus start), so that each new candidate can be compared
			// with it without asking HELIOS for its position again.
			usize found_size = 0;

			auto consider = [&](const auto& node) {
				// The walker hands over statements too; only an expression can be the answer.
				if constexpr (std::derived_from<std::remove_cvref_t<decltype(node)>, code::Expr>) {
					const code::Expr& expr = node;
					if (expr.origin.isGenerated()) return;

					auto position_opt = expr.origin.getSourcePosition(ctx);
					if (!position_opt.has_value()) return;
					const auto& position = position_opt.value();

					if (position.getSource().get() != bounds.getSource().get()) return;
					if (position.getStart() < bounds.getStart() || position.getEnd() > bounds.getEnd())
						return;
					if (!covers(position, offset)) return;

					// Children come after their parents, so on a tie the deeper one wins.
					const usize size = position.getEnd() - position.getStart();
					if (found.empty() || size <= found_size) {
						found      = &expr;
						found_size = size;
					}
				}
			};

			code::walkExprTree(root, consider);
			return found;
		}


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
			if (type_result->hasValue()) return type_result->valueOrPanic().sourceTypeName(ctx);
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
			std::string expression_type = expr.expression_type.getSymbolType().sourceTypeName(ctx);

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

			auto found = findInnermostExpression(
				ctx, *hout, offset, top_expression->getSourcePosition().illegalAccess()
			);
			if (found.empty()) return {};

			return describeExpression(ctx, *found.value());
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
