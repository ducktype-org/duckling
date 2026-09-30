#include "hout_stmt_compilation.hpp"

#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/block_expr.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/desugaring/for.hpp>
#include <helios_private/hout_creation/expressions/coercions/coercions.hpp>
#include <helios_private/hout_creation/expressions/coercions/passing.hpp>
#include <helios_private/hout_creation/expressions/operators.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/shorthands/shorthands.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/placeholder.hpp>
#include <lexer/token_common.hpp>
#include <query_framework/query_errors.hpp>

namespace compiler::helios {

	// Forward declaration for processBlock so HoutStmtMaker can call it.
	template<class Container>
	requires std::same_as<Container, pst::CodeBlock>
	      || std::same_as<Container, pst::CodeBlockOrStmt>
	static code::CodeBlock processBlock(
		query::Context& ctx, pst::AccessLocked<Container> container, tsh::SymbolType<> return_type
	);

	/**
	 * @brief Visitor that creates HOUT statements from PST statements.
	 * It is used locally in processBlock.
	 */
	struct HoutStmtMaker final: public pst::PstVisitorPanicky {
		query::Context&   ctx;
		tsh::SymbolType<> return_type;

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

		HoutStmtMaker(query::Context& ctx, tsh::SymbolType<> return_type):
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

				// Move before the coercion, so the coercion knows about the changed value category.
				auto returned = moveReturnedLocal(ctx, expr_hout->clone());

				auto expr_coerced = coerceFromBox(
					ctx, std::move(returned), return_type, pst_expr.unlock(ctx)->getStablePosition()
				);
				if (expr_coerced.hasFailed()) query::throwFailed();

				// Report only if the compilation of the return statement actually succeeded.
				if (expr_hout->expression_type.getValueCategory().mustMove()) {
					ctx.logInt(makeBox<dia::PlaceholderWarning>(
						"A returned value is moved out of implicitly, `move` is not needed "
						"here.",
						pst_expr.unlock(ctx)->getStablePosition()
					));
				}

				output(
					code::ReturnStmt(code::pstOrigin(stmt), std::move(expr_coerced.valueOrPanic()))
				);
			} else {
				const bool returns_unit = return_type.getType() == tsh::getUnitType()
				                       && return_type.getRefKind() == tsh::ReferenceKind::Direct;

				if (not returns_unit) {
					ctx.logInt(makeBox<ReturnWithoutValueError>(
						stmt->getStablePosition(), makeBox<InteractiveType>(ctx, return_type)
					));
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
			auto location_type = location_expr->expression_type.getSymbolType();
			if (location_type.getRefKind() != tsh::ReferenceKind::Direct)
				location_expr = makeBox<code::DerefExpr>(
					ctx, location_expr->origin.generatedFrom(), location_expr->clone()
				);

			if (not location_expr->expression_type.getValueCategory().canBeAssignedTo()) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Left side of assignment must be addressable location",
					var.unlock(ctx)->getStablePosition(),
					"",
					"here"
				));
				query::throwFailed();
				return;
			}

			auto location_mutability = location_type.getMutability();
			if (location_mutability == tsh::Mutability::Immutable) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Left side of assignment can't be immutable.", assignment->getStablePosition()
				));
				query::throwFailed();
				return;
			}

			// 1. Handle regular assignement
			if (op == base::StrID("=")) {
				// The new `SymbolType` of `location_expr` is the location symbol without the
				// ref/box specifier (as it was removed in the DerefExpr constructor). We now
				// coerce the value expr to the type without the ref/box specifier.
				auto new_value_expr_coerced = getHoutOfExprWithExpectedType(
												  ctx,
												  val,
												  location_expr->expression_type.getSymbolType(),
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

			// 2. Handle operation assignement `location X= value`
			// It desugars to `location = location X value`
			CORE_ASSERT(
				op.isAssignment(), "Assignement operator should be present in assignement statement."
			);
			CORE_ASSERT(
				op.value.strView().back() == '=', "Assignement operatos should end with `=`."
			);

			auto stripped_op
				= lexer::Operator(base::StrID(op.value.str().substr(0, op.value.size() - 1)));

			auto                  location   = s.reusable(s.refOf(location_expr->clone()));
			auto                  op_lhs = location->nextUse();
			BoxOrCRef<code::Expr> op_rhs
				= ctx.query<QueryHoutOfExpr>({ val })->valueOrThrow().ref();
			auto value = code::resolveBinaryOperator(
				ctx,
				stripped_op,
				code::pstOrigin(op_wrapped),
				s.deref(std::move(op_lhs)),
				op_rhs->clone(),
				ctx.query<QueryPrimaryCodeScopeFor>({ assignment })
			);
			// TODO: add coercion

			output(code::AssignmentStmt(
				code::pstOrigin(assignment), s.deref(std::move(location)), std::move(value)
			));
			return;
		}

		void visitExprStmt(pst::Access<pst::ExprStmt> stmt) override {
			auto expr_holder_opt = stmt->getExpr().unlockOpt(ctx);
			if (!expr_holder_opt.has_value()) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Internal compiler error: expression statement has no expression holder.",
					stmt->getStablePosition()
				));
				is_failed = true;
				return;
			}

			auto inner_expr_opt = expr_holder_opt.value()->getExpr().unlockOpt(ctx);
			if (!inner_expr_opt.has_value()) {
				ctx.logInt(makeBox<dia::PlaceholderError>(
					"Internal compiler error: expression statement has no inner expression.",
					stmt->getStablePosition()
				));
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
				auto block_body = processBlock(ctx, block_opt.value()->getBlock(), return_type);
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
					code::pstOrigin(stmt), processBlock(ctx, stmt->getThenBody(), return_type)
				));
				return;
			}

			match_optional(stmt->getElseBody()) {
				opt_some(else_body) {
					output(code::BlockStmt(
						code::pstOrigin(stmt), processBlock(ctx, else_body, return_type)
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
			auto bool_type = tsh::SymbolType<>{
				tsh::getBoolType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
			auto condition_holder = stmt->getCondition();
			if (!condition_holder.has_value()) {
				is_failed = true;
				return;
			}

			auto condition = getHoutOfExprWithExpectedType(
								 ctx, condition_holder.value().unlock(ctx)->getExpr(), bool_type
			)
			                     .valueOrThrow();

			auto then_body = processBlock(ctx, stmt->getThenBody(), return_type);

			match_optional(stmt->getElseBody()) {
				opt_some(else_body) {
					output(code::IfStmt(
						code::pstOrigin(stmt),
						std::move(condition),
						std::move(then_body),
						processBlock(ctx, else_body, return_type)
					));
				}
				opt_none {
					output(code::IfStmt(
						code::pstOrigin(stmt), std::move(condition), std::move(then_body)
					));
				}
			}
		}

		void visitWhile(pst::Access<pst::While> stmt) override {
			auto bool_type = tsh::SymbolType<>{
				tsh::getBoolType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
			auto condition_holder = stmt->getCondition();
			if (!condition_holder.has_value()) {
				is_failed = true;
				return;
			}

			auto condition = getHoutOfExprWithExpectedType(
								 ctx, condition_holder.value().unlock(ctx)->getExpr(), bool_type
			)
			                     .valueOrThrow();

			auto body = processBlock(ctx, stmt->getBody(), return_type);

			output(code::WhileStmt(code::pstOrigin(stmt), std::move(condition), std::move(body)));
		}

		void visitVariable(pst::Access<pst::Variable> stmt) override {
			auto symbol = ctx.query<QuerySymbolOfSTMT>(stmt).valueOrThrow();

			auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->valueOrThrow();

			if (stmt->getValue().empty()) {
				// no initial value case

				if (symbol_type.getMutability() == tsh::Mutability::Immutable) {
					ctx.logInt(makeBox<ImmutableVariableNoInitError>(stmt->getStablePosition()));
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
			auto block_body = processBlock(ctx, stmt->getCodeBlock(), return_type);
			output(code::BlockStmt(code::pstOrigin(stmt), std::move(block_body)));
		}

		void visitConst(pst::Access<pst::Const>) override {
			// Consts inside functions do not produce any HOUT statement.
			// They are translated to HOUT global data instead.
		}

		void visitContinue(pst::Access<pst::Continue> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`continue` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitBreak(pst::Access<pst::Break> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`break` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitRedo(pst::Access<pst::Redo> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`redo` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitThrow(pst::Access<pst::Throw> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`throw` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitDefer(pst::Access<pst::Defer> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`defer` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitRestart(pst::Access<pst::Restart> stmt) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"`restart` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitFor(pst::Access<pst::For> stmt) override {
			auto result = desugaring::desugarFor(
				ctx,
				stmt,
				[&](pst::AccessLocked<pst::CodeBlockOrStmt> body_pst) -> code::CodeBlock {
					return processBlock(ctx, body_pst, return_type);
				}
			);
			if (!result.has_value()) {
				is_failed = true;
				return;
			}
			output(std::move(result.value()));
		}

		void visitFun(pst::Access<pst::Fun> function) override {
			ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
				"Nested functions are not supported yet.", function->getStablePosition()
			));
			is_failed = true;
		}
	};

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
		query::Context& ctx, pst::AccessLocked<Container> container, tsh::SymbolType<> return_type
	) {
		code::CodeBlock block({});
		for (const auto& stmt: getStmtsFromStmtAggregate(ctx, container)) {
			HoutStmtMaker stmt_maker(ctx, return_type);
			auto          unlocked = stmt.unlockOpt(ctx);
			if (!unlocked.has_value()) {
				// @TODO: #1753 change here to grab errors from all statements.
				query::throwFailed();
			}
			unlocked.value()->acceptVisitor(stmt_maker);

			if (stmt_maker.is_failed) {
				// @TODO: #1753 change here to grab errors from all statements.
				query::throwFailed();
			}

			for (auto& prefix_stmt: stmt_maker.prefix_stmts)
				block.statements.emplace_back(std::move(prefix_stmt));

			if (stmt_maker.out.has_value())
				block.statements.emplace_back(std::move(stmt_maker.out.value()));
		}
		return block;
	}

	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                         ctx,
		pst::AccessLocked<pst::CodeBlockOrStmt> container,
		tsh::SymbolType<>                       return_type
	) {
		return std::make_shared<const code::CodeBlock>(processBlock(ctx, container, return_type));
	}

	std::shared_ptr<const code::CodeBlock> compileCodeOfCodeBlock(
		query::Context&                   ctx,
		pst::AccessLocked<pst::CodeBlock> container,
		tsh::SymbolType<>                 return_type
	) {
		return std::make_shared<const code::CodeBlock>(processBlock(ctx, container, return_type));
	}

	code::CodeBlock compileSingleStatement(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, tsh::SymbolType<> return_type
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
