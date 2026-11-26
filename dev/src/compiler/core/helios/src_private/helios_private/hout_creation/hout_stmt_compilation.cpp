// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "hout_stmt_compilation.hpp"

#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/block_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/identifier_literal.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/import.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/specifier_block.hpp>
#include <frontend/pst_parser/visitors/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/value_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/desugaring/for.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/hout_of_subexpr.hpp>
#include <helios_private/hout_creation/expressions/operators.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/pst_layer/unlock_or_fail.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/str/str_utils.hpp>

#include <diagnostic/placeholder.hpp>
#include <lexer/token_common.hpp>
#include <query_framework/query_errors.hpp>

#include <algorithm>
#include <string_view>
#include <variant>

namespace compiler::helios {

	// Forward declaration for processBlock so HoutStmtMaker can call it.
	template<class Container>
	requires std::same_as<Container, pst::CodeBlock>
	      || std::same_as<Container, pst::CodeBlockOrStmt>
	static code::CodeBlock processBlock(
		query::Context&                         ctx,
		pst::Access<Container>                  container,
		const base::Optional<tsh::ValueType<>>& return_type
	);

	template<std::derived_from<pst::Stmt> T>
	static base::Optional<SymID> getControlFlowID(query::Context& ctx, pst::Access<T> stmt) {
		if (!stmt->getDeclSymbolIdentifier().has_value()) return {};

		return ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();
	}

	static base::Optional<code::ControlFlowKind> controlFlowKind(lang_def::Keyword keyword) {
		switch (keyword) {
		case lang_def::Keyword::If:
			return code::ControlFlowKind::If;
		case lang_def::Keyword::While:
			return code::ControlFlowKind::While;
		case lang_def::Keyword::For:
			return code::ControlFlowKind::For;
		case lang_def::Keyword::Block:
			return code::ControlFlowKind::Block;
		default:
			return {};
		}
	}

	template<std::derived_from<pst::Action> T>
	static code::ControlFlowTargetSelector getControlFlowTarget(
		query::Context& ctx, pst::Access<T> stmt
	) {
		auto target_holder  = stmt->getValue();
		auto target_keyword = stmt->getTargetKeyword();
		CORE_ASSERT(
			!target_holder.has_value() || !target_keyword.has_value(),
			"Control-flow target cannot be both a keyword and an expression."
		);
		if (target_keyword.has_value()) {
			auto kind = controlFlowKind(target_keyword.value());
			CORE_ASSERT(kind.has_value(), "Invalid parsed control-flow target keyword.");
			return code::KindTarget{ kind.value() };
		}
		if (target_holder.has_value()) {
			auto target_expr = target_holder.value().unlock(ctx)->getExpr().unlock(ctx);
			auto identifier  = target_expr.template dynamicCast<pst::expr::IdentifierLiteral>();
			CORE_ASSERT(identifier.has_value(), "Parsed control-flow target must be a bare name.");
			return code::NamedTarget{ identifier.value()->getName().unlock(ctx)->unwrap() };
		}
		return code::NearestLoop{};
	}

	/**
	 * @brief Visitor that creates HOUT statements from PST statements.
	 * It is used locally in processBlock.
	 */
	struct HoutStmtMaker final: public pst::PstVisitorPanicky {
		query::Context& ctx;

		base::Optional<tsh::ValueType<>> return_type;

		/**
		 * Whether the statement generation has failed.
		 */
		bool is_failed = false;

		/**
		 * The output statement.
		 * If is_failed is false, but out is empty, it means that the PST statement
		 * did not produce any HOUT statement (e.g., alias or using).
		 */
		base::Optional<Box<code::Stmt>> out;

		/**
		 * Statements to emit before `out`, used by desugarings that need more than one
		 * statement (e.g. a `match` initializer declares the target variable first).
		 */
		std::vector<Box<code::Stmt>> prefix_stmts;

		HoutStmtMaker(query::Context& ctx, base::Optional<tsh::ValueType<>> return_type):
			  ctx(ctx),
			  return_type(return_type) {}

		// @TODO: #1710 visits for all valid stmt-s

		// @TODO: #1710 some stuff in here are also symbols (like named if's)
		// "query symbol in scope" should be able to just work
		// and provide correct symbols for lookup, but some care
		// has to be taken, to ensure consistency between this code and scope states.

		template<class T>
		void output(T&& value) {
			this->out.emplace(makeBox<std::remove_reference_t<T>>(std::forward<T>(value)));
		}

		void visitReturn(pst::Access<pst::Return> stmt) override {
			if (auto val = stmt->getValue()) {
				const auto pst_expr = val.value().unlock(ctx)->getExpr();

				auto expr_hout_qresult = ctx.query<QueryHoutOfExpr>({ pst_expr });
				if (expr_hout_qresult->hasFailed()) {
					is_failed = true;
					return;
				}

				const auto& expr_hout = expr_hout_qresult->valueOrThrow();

				// The returned local dies with the function, so we have to mark it as `LastUse` and
				// implicitly move it.
				const auto position    = pst_expr.unlock(ctx)->getStablePosition();
				const auto target_type = return_type.has_value()
				                           ? return_type.value()
				                           : expr_hout->expression_type.getValueType();
				const query::QResult<coercions::Coercion> coercion = coercions::validCoercionOf(
					ctx, expr_hout.ref(), target_type, position, {}, {}, coercions::HandOverKind::LastUse
				);
				if (coercion.hasFailed()) query::throwFailed();

				// Report only if the compilation of the return statement actually succeeded.
				if (expr_hout->expression_type.getValueCategory().mustMove()) {
					ctx.log<dia::PlaceholderWarning>(
						"A returned value is moved out of implicitly, `move` is not needed "
						"here.",
						position
					);
				}

				output(code::ReturnStmt(
					code::pstOrigin(stmt),
					coercions::applyCoercionFromRef(ctx, coercion.valueOrThrow(), expr_hout.ref())
				));
			} else {
				const bool returns_unit
					= !return_type.has_value()
				   || (return_type.value().getType() == tsh::getUnitType()
				       && return_type.value().getRefKind() == tsh::ReferenceKind::Direct);

				if (not returns_unit) {
					ctx.log<ReturnWithoutValueError>(
						stmt->getStablePosition(), makeBox<InteractiveType>(ctx, return_type.value())
					);
					is_failed = true;
					return;
				}

				output(code::VoidReturnStmt(code::pstOrigin(stmt)));
			}
		}

		void visitUsing(pst::Access<pst::Using>) override {}

		void handleAssignmentExpr(pst::Access<pst::expr::Assignment> assignment) {
			using namespace compiler::helios::code::shorthands;
			Shorthand s{ ctx };

			auto op_wrapped = assignment->getAssignmentType().unlock(ctx);
			auto op         = op_wrapped->unwrap();

			auto var = assignment->getVariables();
			auto val = assignment->getValue();

			BoxOrCRef<code::Expr> location_expr
				= ctx.query<QueryHoutOfExpr>({ var })->valueOrThrow().ref();


			// If left side of the assignment is a ref/box, we have to dereference it and store
			// the value in the memory pointed by the ref/box.
			auto location_type = location_expr->expression_type.getValueType();
			if (location_type.getRefKind() != tsh::ReferenceKind::Direct)
				location_expr = makeBox<code::DerefExpr>(
					ctx, location_expr->origin.generatedFrom(), location_expr->clone()
				);

			if (not location_expr->expression_type.getValueCategory().canBeAssignedTo()) {
				ctx.log<dia::PlaceholderError>(
					"Left side of assignment must be addressable location",
					var.unlock(ctx)->getStablePosition(),
					"",
					"here"
				);
				query::throwFailed();
				return;
			}

			auto location_mutability = location_type.getMutability();
			if (location_mutability == tsh::Mutability::Immutable) {
				ctx.log<dia::PlaceholderError>(
					"Left side of assignment can't be immutable.", assignment->getStablePosition()
				);
				query::throwFailed();
				return;
			}

			// 1. Handle regular assignement
			if (op == base::StrID("=")) {
				// The new `ValueType` of `location_expr` is the location symbol without the
				// ref/box specifier (as it was removed in the DerefExpr constructor). We now
				// coerce the value expr to the type without the ref/box specifier.
				auto new_value_expr_coerced = getHoutOfExprWithExpectedType(
												  ctx,
												  val,
												  location_expr->expression_type.getValueType(),
												  var.unlock(ctx)->getStablePosition()
				)
				                                  .valueOrThrow();

				output(code::AssignmentStmt(
					code::pstOrigin(assignment),
					std::move(location_expr),
					std::move(new_value_expr_coerced)
				));
				return;
			}

			// 2. Handle custom assignement operator
			// @TODO: #3702 implement this

			// 3. Fallback to desugaring
			output(code::desugarAssignmentOperator(ctx, assignment));
			return;
		}

		void visitExprStmt(pst::Access<pst::ExprStmt> stmt) override {
			auto expr_holder_opt = stmt->getExpr().unlockOpt(ctx);
			if (!expr_holder_opt.has_value()) {
				ctx.log<dia::PlaceholderError>(
					"Internal compiler error: expression statement has no expression holder.",
					stmt->getStablePosition()
				);
				is_failed = true;
				return;
			}

			auto inner_expr_opt = expr_holder_opt.value()->getExpr().unlockOpt(ctx);
			if (!inner_expr_opt.has_value()) {
				ctx.log<dia::PlaceholderError>(
					"Internal compiler error: expression statement has no inner expression.",
					stmt->getStablePosition()
				);
				is_failed = true;
				return;
			}

			auto inner_expr = inner_expr_opt.value();

			// here if we encounter an assignment expression
			// we should create an assignment statement:
			if (auto assignment_opt = inner_expr.dynamicCast<pst::expr::Assignment>()) {
				handleAssignmentExpr(assignment_opt.value());
				return;
			}
			// as before, if we encounter an code block expression we want a block statement
			if (auto block_opt = inner_expr.dynamicCast<pst::expr::BlockExpr>()) {
				auto block_body = processBlock(
					ctx, unlockOrFail(ctx, block_opt.value()->getBlock()), return_type
				);
				output(code::BlockStmt(code::pstOrigin(stmt), std::move(block_body)));
				return;
			}

			// else just create an expression statement:

			auto expr = ctx.query<QueryHoutOfExpr>({ inner_expr })->valueOrThrow().ref();
			output(code::ExprStmt(code::pstOrigin(stmt), expr));
		}

		/**
		 * @brief Compiles `if const (...)`, evaluating the condition at compile time and
		 * compiling only the taken branch.
		 *
		 * The branch that is not taken is never lowered to HOUT, so it may contain code that
		 * would not compile for the current instantiation.
		 */
		void compileConstIf(pst::Access<pst::If> stmt) {
			auto condition_holder = stmt->getCondition();
			if (!condition_holder.has_value()) {
				is_failed = true;
				return;
			}

			auto taken = getBoolCTVFromPST(ctx, condition_holder.value().unlock(ctx)->getExpr());
			if (taken.hasFailed()) {
				is_failed = true;
				return;
			}

			if (taken.valueOrThrow()) {
				output(code::BlockStmt(
					code::pstOrigin(stmt),
					processBlock(ctx, unlockOrFail(ctx, stmt->getThenBody()), return_type),
					getControlFlowID(ctx, stmt),
					code::ControlFlowKind::If
				));
				return;
			}

			match_optional(stmt->getElseBody()) {
				opt_some(else_body) {
					output(code::BlockStmt(
						code::pstOrigin(stmt),
						processBlock(ctx, unlockOrFail(ctx, else_body), return_type),
						getControlFlowID(ctx, stmt),
						code::ControlFlowKind::If
					));
				}
				opt_none {
					// Nothing is emitted: neither branch is compiled.
				}
			}
		}

		void visitIf(pst::Access<pst::If> stmt) override {
			if (stmt->isConst()) {
				compileConstIf(stmt);
				return;
			}

			// in the future we must also handle here different if-s variants
			// for example: `if (let a = ...) {}`.
			auto bool_type        = tsh::ValueType<>::withDefaultsMut(tsh::getBoolType());
			auto condition_holder = stmt->getCondition();
			if (!condition_holder.has_value()) {
				is_failed = true;
				return;
			}

			auto condition = getHoutOfExprWithExpectedType(
								 ctx, condition_holder.value().unlock(ctx)->getExpr(), bool_type
			)
			                     .valueOrThrow();

			auto control_flow_id = getControlFlowID(ctx, stmt);
			auto then_body = processBlock(ctx, unlockOrFail(ctx, stmt->getThenBody()), return_type);

			match_optional(stmt->getElseBody()) {
				opt_some(else_body) {
					output(code::IfStmt(
						code::pstOrigin(stmt),
						std::move(condition),
						std::move(then_body),
						processBlock(ctx, unlockOrFail(ctx, else_body), return_type),
						control_flow_id
					));
				}
				opt_none {
					output(code::IfStmt(
						code::pstOrigin(stmt),
						std::move(condition),
						std::move(then_body),
						control_flow_id
					));
				}
			}
		}

		void visitWhile(pst::Access<pst::While> stmt) override {
			auto bool_type        = tsh::ValueType<>::withDefaultsMut(tsh::getBoolType());
			auto control_flow_id  = getControlFlowID(ctx, stmt);
			auto condition_holder = stmt->getCondition();
			if (!condition_holder.has_value()) {
				is_failed = true;
				return;
			}

			auto condition = getHoutOfExprWithExpectedType(
								 ctx, condition_holder.value().unlock(ctx)->getExpr(), bool_type
			)
			                     .valueOrThrow();

			auto body = processBlock(ctx, unlockOrFail(ctx, stmt->getBody()), return_type);

			output(code::WhileStmt(
				code::pstOrigin(stmt), std::move(condition), std::move(body), control_flow_id
			));
		}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			auto symbol = ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();

			auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow();

			if (stmt->getValue().empty()) {
				// no initial value case

				if (symbol_type.getMutability() == tsh::Mutability::Immutable) {
					ctx.log<ImmutableVariableNoInitError>(stmt->getStablePosition());
					is_failed = true;
					return;
				}

				auto initial_value_qresult
					= defgen::getDefaultInitializerExpr(ctx, symbol_type, stmt->getStablePosition());
				if (initial_value_qresult.hasFailed()) {
					is_failed = true;
					return;
				}
				auto initial_value = initial_value_qresult.valueOrThrow();

				output(code::VariableStmt(code::pstOrigin(stmt), initial_value, symbol_type, symbol)
				);
			} else {
				auto initial_value_coerced = getHoutOfExprWithExpectedType(
												 ctx,
												 stmt->getValue().value().unlock(ctx)->getExpr(),
												 symbol_type,
												 stmt->getName().unlock(ctx)->getStablePosition()
				)

				                                 .valueOrThrow();


				output(code::VariableStmt(
					code::pstOrigin(stmt), std::move(initial_value_coerced), symbol_type, symbol
				));
			}
		}

		void visitBlock(pst::Access<pst::Block> stmt) override {
			auto control_flow_id = getControlFlowID(ctx, stmt);
			auto block_body
				= processBlock(ctx, unlockOrFail(ctx, stmt->getCodeBlock()), return_type);
			output(code::BlockStmt(
				code::pstOrigin(stmt),
				std::move(block_body),
				control_flow_id,
				code::ControlFlowKind::Block
			));
		}

		void visitConst(pst::Access<pst::Const>) override {
			// Consts inside functions do not produce any HOUT statement.
			// They are translated to HOUT global data instead.
		}

		void visitContinue(pst::Access<pst::Continue> stmt) override {
			auto target = getControlFlowTarget(ctx, stmt);
			output(code::ContinueStmt(code::pstOrigin(stmt), target));
		}

		void visitBreak(pst::Access<pst::Break> stmt) override {
			auto target = getControlFlowTarget(ctx, stmt);
			output(code::BreakStmt(code::pstOrigin(stmt), target));
		}

		void visitRedo(pst::Access<pst::Redo> stmt) override {
			ctx.log<dia::NotYetImplementedCodeError>(
				"`redo` statements are not supported yet.", stmt->getStablePosition()
			);
			is_failed = true;
		}

		void visitThrow(pst::Access<pst::Throw> stmt) override {
			ctx.log<dia::NotYetImplementedCodeError>(
				"`throw` statements are not supported yet.", stmt->getStablePosition()
			);
			is_failed = true;
		}

		void visitDefer(pst::Access<pst::Defer> stmt) override {
			ctx.log<dia::NotYetImplementedCodeError>(
				"`defer` statements are not supported yet.", stmt->getStablePosition()
			);
			is_failed = true;
		}

		void visitRestart(pst::Access<pst::Restart> stmt) override {
			ctx.log<dia::NotYetImplementedCodeError>(
				"`restart` statements are not supported yet.", stmt->getStablePosition()
			);
			is_failed = true;
		}

		void visitFor(pst::Access<pst::For> stmt) override {
			auto result = desugaring::desugarFor(
				ctx,
				stmt,
				getControlFlowID(ctx, stmt),
				[&](pst::AccessLocked<pst::CodeBlockOrStmt> body_pst) -> code::CodeBlock {
					return processBlock(ctx, unlockOrFail(ctx, body_pst), return_type);
				}
			);
			if (!result.has_value()) {
				is_failed = true;
				return;
			}
			output(std::move(result.value()));
		}

		void visitFun(pst::Access<pst::Fun> function) override {
			failNotSupportedInFunction("Nested functions are", function);
		}

		void visitFunDecl(pst::Access<pst::FunDecl> fun_decl) override {
			failNotSupportedInFunction("Nested function declarations are", fun_decl);
		}

		void visitClass(pst::Access<pst::Class> class_decl) override {
			failNotSupportedInFunction("Classes declared inside a function are", class_decl);
		}

		void visitNamespace(pst::Access<pst::Namespace> namespace_decl) override {
			failNotSupportedInFunction("Namespaces declared inside a function are", namespace_decl);
		}

		void visitTemplateStmt(pst::Access<pst::TemplateStmt> template_stmt) override {
			failNotSupportedInFunction("Templates declared inside a function are", template_stmt);
		}

		void visitPattern(pst::Access<pst::Pattern> pattern) override {
			failNotSupportedInFunction("Patterns declared inside a function are", pattern);
		}

		void visitImport(pst::Access<pst::Import> import) override {
			failNotAllowedInFunction(
				"`import` statements are not allowed inside a function. Move it to the module "
				"level.",
				import
			);
		}

		void visitSpecifierBlock(pst::Access<pst::SpecifierBlock> block) override {
			if (hasSpecifier(block, pst::Keyword::Export)) {
				failNotAllowedInFunction(
					"`export` blocks are not allowed inside a function. Only module-level "
					"declarations can be exported.",
					block
				);
				return;
			}
			failNotSupportedInFunction("Specifier blocks inside a function are", block);
		}

	private:
		/**
		 * @brief Logs a not-yet-implemented error `<what> not supported yet.` at `stmt` and marks
		 * the statement as failed.
		 */
		template<class Element>
		void failNotSupportedInFunction(std::string_view what, pst::Access<Element> stmt) {
			ctx.log<dia::NotYetImplementedCodeError>(
				base::strConcat(what, " not supported yet."), stmt->getStablePosition()
			);
			is_failed = true;
		}

		/**
		 * @brief Logs an error for a statement that is never allowed inside a function body (by
		 * language design, not a missing feature) and marks the statement as failed.
		 */
		template<class Element>
		void failNotAllowedInFunction(std::string message, pst::Access<Element> stmt) {
			ctx.log<dia::PlaceholderError>(std::move(message), stmt->getStablePosition());
			is_failed = true;
		}

		/**
		 * @brief Tells whether `stmt` carries the specifier `keyword`.
		 */
		template<class Element>
		bool hasSpecifier(pst::Access<Element> stmt, pst::Keyword keyword) {
			return std::ranges::any_of(stmt->getSpecifiers(), [&](const auto& specifier) {
				return specifier.unlock(ctx)->getSpecifier().unlock(ctx)->unwrap() == keyword;
			});
		}
	};

	/**
	 * @brief Compiles `stmt` and appends what it produced to `block`.
	 */
	static void appendStmt(
		query::Context&                         ctx,
		code::CodeBlock&                        block,
		pst::Access<pst::Stmt>                  stmt,
		const base::Optional<tsh::ValueType<>>& return_type
	) {
		HoutStmtMaker stmt_maker(ctx, return_type);
		stmt->acceptVisitor(stmt_maker);

		if (stmt_maker.is_failed) query::throwFailed();

		for (auto& prefix_stmt: stmt_maker.prefix_stmts)
			block.statements.emplace_back(std::move(prefix_stmt));

		if (stmt_maker.out.has_value())
			block.statements.emplace_back(std::move(stmt_maker.out.value()));
	}

	/**
	 * @brief Compiles a PST code block or statement into a HOUT CodeBlock.
	 *
	 * Iterates all statements in the container via HoutStmtMaker and returns the
	 * resulting CodeBlock by value. Called from the compileCodeOfCodeBlock
	 * helper and recursively within HoutStmtMaker for nested blocks (if/while bodies).
	 */
	template<class Container>
	requires std::same_as<Container, pst::CodeBlock>
	      || std::same_as<Container, pst::CodeBlockOrStmt>
	static code::CodeBlock processBlock(
		query::Context&                         ctx,
		pst::Access<Container>                  container,
		const base::Optional<tsh::ValueType<>>& return_type
	) {
		code::CodeBlock block({});
		for (const auto& stmt: getStmtsFromStmtAggregate(ctx, container))
			appendStmt(ctx, block, stmt, return_type);
		return block;
	}

	query::QResult<Box<code::Expr>> compileBlockExpr(
		query::Context& ctx, pst::Access<pst::expr::BlockExpr> block_expr
	) {
		const auto origin = code::pstOrigin(block_expr);
		auto contents = getStmtsFromStmtAggregate(ctx, unlockOrFail(ctx, block_expr->getBlock()));

		base::Optional<Box<code::Expr>> tail;
		if (!contents.empty() && contents.back()->isImplicitReturn()) {
			if (auto expr_stmt_opt = contents.back().dynamicCast<pst::ExprStmt>(); expr_stmt_opt) {
				auto tail_holder = expr_stmt_opt.value()->getExpr().unlock(ctx);
				UNPACK_QRESULT_MOVE(
					auto tail_expr =,
					code::subExprFromPST(ctx, unlockOrFail(ctx, tail_holder->getExpr()))
				);
				const auto tail_type = tail_expr->expression_type.getValueType();
				UNPACK_QRESULT_MOVE(
					tail =,
					coercions::coerceFromBox(
						ctx, std::move(tail_expr), tail_type, tail_holder->getStablePosition()
					)
				);
				contents.pop_back();
			}
		}

		code::CodeBlock body({});
		for (const auto& stmt: contents) appendStmt(ctx, body, stmt, {});

		return Box<code::Expr>(makeBox<code::BlockExpr>(
			origin, makeBox<code::BlockStmt>(origin, std::move(body)), std::move(tail)
		));
	}

	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                   ctx,
		pst::Access<pst::CodeBlockOrStmt> container,
		tsh::ValueType<>                  return_type
	) {
		return std::make_shared<const code::CodeBlock>(processBlock(ctx, container, return_type));
	}

	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                   ctx,
		pst::AccessLocked<pst::CodeBlock> container,
		tsh::ValueType<>                  return_type
	) {
		return std::make_shared<const code::CodeBlock>(
			processBlock(ctx, unlockOrFail(ctx, container), return_type)
		);
	}

	code::CodeBlock compileSingleStatement(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, tsh::ValueType<> return_type
	) {
		code::CodeBlock block({});
		HoutStmtMaker   stmt_maker(ctx, return_type);

		auto unlocked = stmt.unlockOpt(ctx);
		if (!unlocked.has_value()) query::throwFailed();

		unlocked.value()->acceptVisitor(stmt_maker);

		if (stmt_maker.is_failed) query::throwFailed();

		for (auto& prefix_stmt: stmt_maker.prefix_stmts)
			block.statements.emplace_back(std::move(prefix_stmt));

		if (stmt_maker.out.has_value())
			block.statements.emplace_back(std::move(stmt_maker.out.value()));

		return block;
	}

}
