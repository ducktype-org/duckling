#include "hout_stmt_compilation.hpp"

#include "ctv/numeric_value.hpp"
#include "helios/hout/elements/expr.hpp"
#include "helios/hout/elements/stmt.hpp"
#include "helios/tsh/kind.hpp"
#include "helios/tsh/mutability.hpp"
#include "helios/tsh/types.hpp"
#include "helios_private/scopes/scopes.hpp"
#include "helios_private/symbols/generated_symbol_data.hpp"

#include <diagnostic_interactive/placeholder.hpp>
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
#include <helios/hout/origin.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "base/except/exceptions.hpp"
#include "base/pointers/box.hpp"
#include "base/str/str_utils.hpp"
#include <base/collections/optional.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include "string_id/string_id.hpp"
#include <query_framework/query_errors.hpp>

#include <algorithm>

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
				auto expr_coerced = getHoutOfExprWithExpectedType(
										ctx, val.value().unlock(ctx)->getExpr(), return_type
				)
				                        .valueOrThrow();
				output(code::ReturnStmt(code::pstOrigin(stmt), std::move(expr_coerced)));
			} else {
				output(code::VoidReturnStmt(code::pstOrigin(stmt)));
			}
		}

		void visitAlias(pst::Access<pst::Alias>) override {}

		void visitUsing(pst::Access<pst::Using>) override {}

		void handleAssignmentExpr(pst::Access<pst::expr::Assignment> assignment) {
			auto op = assignment->getAssignmentType().unlock(ctx)->unwrap();
			if (op != base::StrID("=") && op != base::StrID("+=") && op != base::StrID("-=")) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat("This assignment type: '", op.str(), "'."),
					assignment->getStablePosition()
				));
				query::throwFailed();
			}

			if (op != base::StrID("=")) {
				auto assignment_expr
					= ctx.query<QueryHoutOfExpr>({ assignment })->valueOrThrow().ref();
				output(code::ExprStmt(code::pstOrigin(assignment), assignment_expr));
				return;
			}

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

			auto location_value_category
				= location_expr->expression_type.getValueCategory().getCategory();
			if (location_value_category == tsh::PrimaryCategory::Literal) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Left side of assignment is a literal",
					var.unlock(ctx)->getStablePosition(),
					"",
					"here"
				));
				query::throwFailed();
				return;
			}

			auto location_mutability = location_type.getMutability();
			if (location_mutability == tsh::Mutability::Immutable) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Left side of assignment can't be immutable.", assignment->getStablePosition()
				));
				query::throwFailed();
				return;
			}

			if (op == base::StrID("=")) {
				// The new `SymbolType` of `location_expr` is the location symbol without the
				// ref/box specifier (as it was removed in the DerefExpr constructor). We now
				// coerce the value expr to the type without the ref/box specifier.
				auto new_value_expr_coerced
					= getHoutOfExprWithExpectedType(
						  ctx, val, location_expr->expression_type.getSymbolType()
					)
				          .valueOrThrow();

				output(code::AssignmentStmt(
					code::pstOrigin(assignment),
					std::move(location_expr),
					std::move(new_value_expr_coerced)
				));
				return;
			}

			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				base::strConcat(
					"'", op.str(), "' assignment for type: '", location_type.toString(), "'."
				),
				assignment->getStablePosition()
			));
			query::throwFailed();
		}

		void visitExprStmt(pst::Access<pst::ExprStmt> stmt) override {
			auto expr_holder_opt = stmt->getExpr().unlockOpt(ctx);
			if (!expr_holder_opt.has_value()) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Internal compiler error: expression statement has no expression holder.",
					stmt->getStablePosition()
				));
				is_failed = true;
				return;
			}

			auto inner_expr_opt = expr_holder_opt.value()->getExpr().unlockOpt(ctx);
			if (!inner_expr_opt.has_value()) {
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
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

		void visitIf(pst::Access<pst::If> stmt) override {
			// in the future we must also handle here different if-s variants
			// for example: `if (let a = ...) {}`.
			auto bool_type = tsh::SymbolType<>{
				tsh::getBoolType(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
			auto condition = getHoutOfExprWithExpectedType(
								 ctx, stmt->getCondition().unlock(ctx)->getExpr(), bool_type
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
			auto condition = getHoutOfExprWithExpectedType(
								 ctx, stmt->getCondition().unlock(ctx)->getExpr(), bool_type
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
				auto initial_value_coerced
					= getHoutOfExprWithExpectedType(
						  ctx, stmt->getValue().value().unlock(ctx)->getExpr(), symbol_type
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
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`continue` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitBreak(pst::Access<pst::Break> stmt) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`break` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitRedo(pst::Access<pst::Redo> stmt) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`redo` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitThrow(pst::Access<pst::Throw> stmt) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`throw` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitDefer(pst::Access<pst::Defer> stmt) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`defer` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitRestart(pst::Access<pst::Restart> stmt) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
				"`restart` statements are not supported yet.", stmt->getStablePosition()
			));
			is_failed = true;
		}

		void visitFor(pst::Access<pst::For> stmt) override {
			std::cout << "======== LOWERING FOR ========\n";
			stmt->debugPrint(std::cout);
			std::cout << '\n';

			// Preamble. Get some basic data.
			using GeneratedSymbolData = defgen::GeneratedSymbolData;
			using ControlFlowLocal    = GeneratedSymbolData::ControlFlowLocal;

			const auto loop_origin = code::pstOrigin(stmt);
			const auto gen_origin  = code::generatedOrigin();


			const ScopeID for_scope = ctx.query<QueryPrimaryCodeScopeFor>({ stmt });

			// Get the iterable and it's type.
			auto iterable_pst  = stmt->getIterable().unlock(ctx)->getExpr();
			auto iterable_hout = ctx.query<QueryHoutOfExpr>({ iterable_pst })->valueOrThrow().ref();
			auto iterable_type = iterable_hout->expression_type.getSymbolType();
			auto iterable_kind = iterable_type.getType().getKind();

			if (iterable_kind != tsh::Kind::DynamicArray
			    && iterable_kind != tsh::Kind::StaticArray) {
				ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
					base::strConcat(
						"`for` statements for non-array type: ", iterable_type.toString()
					),
					stmt->getStablePosition()
				));
				is_failed = true;
				return;
			}

			// Helper types.
			auto u64_mut_type = tsh::SymbolType<>::withDefaults(
				tsh::getIntegralType(ctx, 64, tsh::IntegralAbstractType::Signedness::Unsigned)
			);
			auto u64_immut_type = u64_mut_type.withMutability(tsh::Mutability::Mutable);

			auto element_type = [&]() {
				switch (iterable_kind) {
				case tsh::Kind::DynamicArray:
					return iterable_type.getType().as<tsh::DynamicArrayAbstractType>().getElementType(
					);
				case tsh::Kind::StaticArray:
					return iterable_type.getType().as<tsh::StaticArrayAbstractType>().getElementType(
					);
				default:
					CORE_UNREACHABLE();
				}
			}();

			// Coerce the iterator to the element type.
			auto iter_type_pst = stmt->getIteratorType().unlock(ctx)->getExpr();
			auto iter_type_hout
				= getHoutOfExprWithExpectedType(ctx, iter_type_pst, element_type).valueOrThrow();
			auto iter_type = iter_type_hout->expression_type.getSymbolType();


			// Create the needed symbols.
			auto create_var = [&](base::StrID role, tsh::SymbolType<> type) {
				return ctx.query<defgen::QueryGeneratedSymbol>({ .name = role,
				                                                 .generated_symbol_data
				                                                 = GeneratedSymbolData{
																	 ControlFlowLocal{
																		 .owning_scope = for_scope,
																		 .role         = role,
																		 .type         = type,
																	 },
																 } });
			};


			// Create needed symbols.
			SymID col_sym  = create_var(base::StrID("__collection"), iterable_type);
			SymID idx_sym  = create_var(base::StrID("__index"), u64_mut_type);
			SymID len_sym  = create_var(base::StrID("__len"), u64_immut_type);
			SymID iter_sym = getForIteratorSymbol(ctx, stmt);

			// Desugar the loop.
			code::CodeBlock outer{};
			// Create a temp for the collection for it to be evaluated only once before the loop.
			// var __collection = <iterable>
			outer.statements.emplace_back(
				makeBox<code::VariableStmt>(loop_origin, iterable_hout, iterable_type, col_sym)
			);

			// var __idx: u64 = 0
			outer.statements.emplace_back(makeBox<code::VariableStmt>(
				loop_origin,
				makeBox<code::DefaultValueExpr>(ctx, code::generatedOrigin(), u64_mut_type.getType()),
				u64_mut_type,
				idx_sym
			));

			// let __len: u64 = <len __collection> / <constant>
			auto len_expr = [&]() -> Box<code::Expr> {
				switch (iterable_kind) {
				case tsh::Kind::DynamicArray: {
					return makeBox<code::UnaryOperatorExpr>(
						ctx,
						code::generatedOrigin(),
						code::BuiltinUnary::Len,
						makeBox<code::IdentifierExpr>(ctx, gen_origin, col_sym)
					);
				}
				case tsh::Kind::StaticArray: {
					auto size
						= iterable_type.getType().as<tsh::StaticArrayAbstractType>().getSize();
					return makeBox<code::LiteralNumericExpr>(
						ctx,
						gen_origin,
						numeric_value::NumericValue::createOfType<u64>(
							u64_immut_type.getType(), size
						)
							.value()
					);
				}
				default:
					CORE_UNREACHABLE();
				}
			}();
			outer.statements.emplace_back(makeBox<code::VariableStmt>(
				loop_origin, std::move(len_expr), u64_immut_type, len_sym
			));

			// while(__idx < __len) {
			// 		let <iter> = __collection[__idx];
			// 		<body>;
			// 		__idx = __idx + 1;
			// }

			// Prepare the condition.
			auto idx_ref = [&] { return makeBox<code::IdentifierExpr>(ctx, gen_origin, idx_sym); };
			auto len_ref = [&] { return makeBox<code::IdentifierExpr>(ctx, gen_origin, len_sym); };
			auto col_ref = [&] { return makeBox<code::IdentifierExpr>(ctx, gen_origin, col_sym); };

			// __idx < __len
			auto condition = makeBox<code::BinaryOperatorExpr>(
				ctx, gen_origin, code::BuiltinBinary::IntegerLt, idx_ref(), len_ref()
			);

			// Prepare the body.
			code::CodeBlock while_body{};

			// let <user_var> = __collection[__idx];
			while_body.statements.emplace_back(makeBox<code::VariableStmt>(
				loop_origin,
				makeBox<code::IndexExpr>(ctx, loop_origin, col_ref(), idx_ref()),
				iter_type,
				iter_sym
			));

			auto body = processBlock(ctx, stmt->getBody(), return_type);
			for (auto& s: body.statements) while_body.statements.emplace_back(std::move(s));

			// __idx += 1
			auto one = makeBox<code::LiteralNumericExpr>(
				ctx,
				gen_origin,
				numeric_value::NumericValue::createOfType<u64>(u64_mut_type.getType(), 1).value()
			);

			while_body.statements.emplace_back(makeBox<code::AssignmentStmt>(
				gen_origin,
				idx_ref(),
				makeBox<code::BinaryOperatorExpr>(
					ctx, gen_origin, code::BuiltinBinary::IntegerAdd, idx_ref(), std::move(one)
				)
			));


			outer.statements.emplace_back(
				makeBox<code::WhileStmt>(loop_origin, std::move(condition), std::move(while_body))
			);

			// Emit it in a block so variable names don't collide if two fors are in the same function.
			output(code::BlockStmt(loop_origin, std::move(while_body)));
		}

		void visitFun(pst::Access<pst::Fun> function) override {
			ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
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

	code::CodeBlock compileSingleStatement(
		query::Context& ctx, pst::AccessLocked<pst::Stmt> stmt, tsh::SymbolType<> return_type
	) {
		code::CodeBlock block({});
		HoutStmtMaker   stmt_maker(ctx, return_type);

		auto unlocked = stmt.unlockOpt(ctx);
		if (!unlocked.has_value()) query::throwFailed();

		unlocked.value()->acceptVisitor(stmt_maker);

		if (stmt_maker.is_failed) query::throwFailed();

		if (stmt_maker.out.has_value())
			block.statements.emplace_back(std::move(stmt_maker.out.value()));

		return block;
	}

}
