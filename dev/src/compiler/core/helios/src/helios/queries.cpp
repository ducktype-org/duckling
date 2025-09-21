#include "queries.hpp"

#include <frontend/module_tree/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/actions/return.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <pst_parser/pst_visitor.hpp>

#include <base/exceptions.hpp>
#include <base/stable_hashmap.hpp>

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryModuleHOUT, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto scopes = ctx.query<QueryScopesInModule>(key);

			HOUTUnit out;
			for (auto scope: *scopes) {
				auto symbols_in_scope = ctx.query<QuerySymbolsInScope>(scope);

				for (auto sym: *symbols_in_scope) {
					// grab constants:
					if (kind(sym) == SymbolKind::Const)
						out.glob_data.emplace_back(sym, ctx, HOUTGlobalDataType::Constant);
					if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
						out.glob_data.emplace_back(sym, ctx, HOUTGlobalDataType::Variable);
					// grab functions:
					if (kind(sym) == SymbolKind::Function)
						out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));
				}
			}
			return out;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUT);

	struct IMPLEMENT_QUERY(QueryModuleHOUTRecursively, std::vector<HOUTUnit>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<HOUTUnit> out = { ctx.query<QueryModuleHOUT>(key) };

			auto submodules = ctx.query<frontend::QuerySubmodules>(key);
			for (auto submodule: *submodules) {
				// @TODO optimize multiple concatenations
				auto submodule_hout = ctx.query<QueryModuleHOUTRecursively>(submodule.second);
				for (const auto& i: submodule_hout) out.push_back(i);
			}
			return out;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUTRecursively);

	struct IMPLEMENT_QUERY(QueryTopLevelEntities, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all top level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto main_file_root_scope = queryRootScopeOfMainModuleFile(ctx, key);

			auto symbols_in_module_root = ctx.query<QuerySymbolsInScope>(main_file_root_scope);

			HOUTUnit out;

			for (auto sym: *symbols_in_module_root) {
				// grab constants:
				if (kind(sym) == SymbolKind::Const)
					out.glob_data.emplace_back(sym, ctx, HOUTGlobalDataType::Constant);
				if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
					out.glob_data.emplace_back(sym, ctx, HOUTGlobalDataType::Variable);
				// grab functions:
				if (kind(sym) == SymbolKind::Function)
					out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));
			}

			return out;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);

	struct IMPLEMENT_QUERY(QueryDeclOfFun, HOUTFunctionDeclaration) {
		struct declarationVisitor final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunctionDeclaration> out;

			declarationVisitor(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			// @TODO: make failure more explicit
			void visitFun(pst::Access<pst::Fun> stmt) final {
				// @TODO: rest, flags, attributes, etc
				HOUTFunction output(original_symbol, ctx);

				std::vector<code::Parameter> parameters;
				for (auto param: *stmt->getParams().unlock(ctx)) {
					auto param_symbol = ctx.query<QuerySymbolOfSTMT>({ param });
					auto param_name   = name(param_symbol);
					auto param_type   = ctx.query<QueryTypeOfSymbol>({ param_symbol });

					auto value = param.unlock(ctx)->getValue();

					if (param_type->hasError()) {
						// we just fail here, because we can't continue without type
						return;
					}

					if (value.empty()) {
						parameters.emplace_back(
							param_name, param_type->value(), std::nullopt, param_symbol
						);
					} else {
						auto initial_value
							= ctx.query<QueryHoutOfExpr>(value.value().unlock(ctx)->getExpr());

						if (initial_value.hasError()) {
							// we just fail here, because we can't continue without correct
							// initial expression
							return;
						}

						parameters.emplace_back(
							param_name,
							param_type->value(),
							std::move(initial_value.value()),
							param_symbol
						);
					}
				}

				output.parameters
					= std::make_shared<std::vector<code::Parameter>>(std::move(parameters));

				this->out.emplace(std::move(output));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function || kind(key) == SymbolKind::BuiltinFunction,
				"Function declaration processing called on non-function symbol"
			);

			declarationVisitor func_maker(ctx, key);
			stmt(ctx, key).value()->acceptVisitor(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_NO_CACHE  // Query is used to provide default argumentes for call processing.
		                     // They are provided in boxes which are moved to call expression. After
		                     // queries are refactored to store refs it should be changed.
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDeclOfFun);

	struct IMPLEMENT_QUERY(QueryCodeOFFun, HOUTFunction) {
		/**
		 * @brief Query extension to get hout CodeBlock from pst::CodeBlock or pst::CodeBlockOrStmt
		 * Might be changed into query in the future
		 */
		template<class Container>
		static auto queryCodeOfCodeBlock(query::Context& ctx, const Container& container) {
			code::CodeBlock block({});
			for (const auto& stmt: *container.unlock(ctx)) {
				HoutStmtMaker stmt_maker(ctx);
				stmt.unlock(ctx)->acceptVisitor(stmt_maker);
				if (not stmt_maker.empty)
					block.statements.emplace_back(std::move(stmt_maker.out.value()));
			}
			return block;
		}

		/**
		 * Creates HOUT code block from single-statement function body.
		 * i.e.: handles the `fun abc() = expr;` case.
		 * Should only be used for the whole function body.
		 */
		static code::CodeBlock queryCodeOfSingleStmtFunctionBody(
			query::Context& ctx, pst::Access<pst::CodeBlockOrStmt> body
		) {
			CORE_ASSERT(body->getType() == pst::CodeBlockOrStmt::Type::SingleStmt, "Bad body type!");

			code::CodeBlock block({});

			auto stmt    = body->getStmt().unlock(ctx);
			auto as_expr = stmt.dynamicCast<pst::ExprStmt>();
			if (as_expr) {
				auto expr
					= ctx.query<QueryHoutOfExpr>(as_expr.value()->getExpr().unlock(ctx)->getExpr())
				          .expect("Not handling errors here yet... (single expr function body)");
				// @TODO #1291 coerce expr to function return type
				block.statements.emplace_back(makeBox<code::ReturnStmt>(std::move(expr)));
				return block;
			} else {
				ctx.log(
					makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
						stmt->getSourcePosition(),
						"Function body in single-statement function must be an expression statement"
					)
				);
				CORE_PANIC("Not handling errors here yet... (single stmt function body)");
			}
		}

		struct HoutStmtMaker final: public pst::PstVisitorPanicky {
			query::Context&                 ctx;
			bool                            empty = false;
			base::Optional<Box<code::Stmt>> out;

			HoutStmtMaker(query::Context& ctx): ctx(ctx) {}

			// @TODO: visits for all valid stmt-s

			// @TODO: some stuff in here are also symbols (like named if's)
			// "query symbol in scope" should be able to just work
			// and provide correct symbols for lookup, but some care
			// has to be taken, to ensure consistency between this code and scope states.

			template<class T>
			void output(T&& value) {
				this->out.emplace(makeBox<std::remove_reference_t<T>>(std::forward<T>(value)));
			}

			void visitReturn(pst::Access<pst::Return> stmt) override {
				if (auto val = stmt->getValue()) {
					auto expr = ctx.query<QueryHoutOfExpr>({ val.value().unlock(ctx)->getExpr() })
					                .expect("Not handling errors here yet... (return expr)");

					// @TODO #1291 coerce expr to function return type
					output(code::ReturnStmt(std::move(expr)));
				} else {
					output(code::VoidReturnStmt());
				}
			}

			void visitAlias(pst::Access<pst::Alias>) override { empty = true; }

			void visitUsing(pst::Access<pst::Using>) override { empty = true; }

			void handleAssignmentExpr(pst::Access<pst::expr::Assignment> assignment) {
				CORE_ASSERT(
					assignment->getAssignmentType() == base::StrID("="),
					"Unsupported assignment type"
				);

				auto var = assignment->getVariables();
				auto val = assignment->getValue();

				auto location_expr = ctx.query<QueryHoutOfExpr>({ var }).expect(
					"Not handling errors here yet... (lhs)"
				);
				auto new_value_expr = ctx.query<QueryHoutOfExpr>({ val }).expect(
					"Not handling errors here yet... (rhs)"
				);

				auto location_value_category
					= location_expr->expression_type.getValueCategory().getCategory();
				if (location_value_category == tsh::PrimaryCategory::Literal) {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							assignment->getSourcePosition(),
							"Left side of assignment can't be a literal."
						)
					);
					return;  // fail
				}

				auto location_type  = location_expr->expression_type.getSymbolType();
				auto new_value_type = new_value_expr->expression_type.getSymbolType();

				auto new_value_coerced = coerceExpression(std::move(new_value_expr), location_type);

				if (new_value_coerced.hasError()) {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							assignment->getSourcePosition(),
							base::strConcat(
								"Bad type passed to assignment\n",
								"Expected: ",
								location_type.toString(),
								"\n",
								"Got: ",
								new_value_type.toString(),
								"\n"
							)
						)
					);
					return;  // fail
				}


				output(code::AssignmentStmt(
					std::move(location_expr), std::move(new_value_coerced.value())
				));
			}

			void visitExprStmt(pst::Access<pst::ExprStmt> stmt) override {
				// @TODO: handle null here
				auto inner_expr = stmt->getExpr().unlock(ctx)->getExpr().unlock(ctx);

				// here if we encounter an assignment expression
				// we should create an assignment statement:
				if (auto assignment_opt = inner_expr.dynamicCast<pst::expr::Assignment>()) {
					handleAssignmentExpr(assignment_opt.value());
					return;
				}

				// else just create an expression statement:

				auto expr = ctx.query<QueryHoutOfExpr>({ inner_expr })
				                .expect("Not handling errors here yet... (ExprStmt)");

				output(code::ExprStmt(std::move(expr)));
			}

			void visitIf(pst::Access<pst::If> stmt) override {
				// in the future we must also handle here different if-s variants
				// for example: `if (let a = ...) {}`.
				auto condition
					= ctx.query<QueryHoutOfExpr>(stmt->getCondition().unlock(ctx)->getExpr())
				          .expect("Not handling errors here yet... (If)");

				auto then_body = queryCodeOfCodeBlock(ctx, stmt->getThenBody());

				match_optional(stmt->getElseBody()) {
					opt_some(else_body) {
						output(code::IfStmt(
							std::move(condition),
							std::move(then_body),
							queryCodeOfCodeBlock(ctx, else_body)
						));
					}
					opt_none { output(code::IfStmt(std::move(condition), std::move(then_body))); }
				}
			}

			void visitWhile(pst::Access<pst::While> stmt) override {
				auto condition
					= ctx.query<QueryHoutOfExpr>(stmt->getCondition().unlock(ctx)->getExpr())
				          .expect("Not handling errors here yet... (While)");

				auto body = queryCodeOfCodeBlock(ctx, stmt->getBody());

				output(code::WhileStmt(std::move(condition), std::move(body)));
			}

			void visitVariable(pst::Access<pst::Variable> stmt) override {
				// @TODO: do something with mut/immut

				auto symbol = ctx.query<QuerySymbolOfSTMT>(stmt);

				auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					"Handling errors is not supported in HOUT yet"
				);

				if (stmt->getValue().empty()) {
					// no initial value case
					output(code::VariableStmt({}, symbol_type, symbol));
					return;
				} else {
					auto initial_value
						= ctx.query<QueryHoutOfExpr>(stmt->getValue().value().unlock(ctx)->getExpr())
					          .expect("Not handling errors here yet... (variable initial value)");

					// used for error reporting:
					auto initial_value_type = initial_value->expression_type.getSymbolType();

					auto initial_value_coerced
						= coerceExpression(std::move(initial_value), symbol_type);
					if (initial_value_coerced.hasError()) {
						ctx.log(makeBox<
								dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							stmt->getValue().value().unlock(ctx)->getSourcePosition(),
							base::strConcat(
								"Bad type passed to variable initialization\n",
								"Expected: ",
								symbol_type.toString(),
								"\n",
								"Got: ",
								initial_value_type.toString(),
								"\n"
							)
						));
						return;  // fail
					}

					output(code::VariableStmt(
						std::move(initial_value_coerced.value()), symbol_type, symbol
					));
				}
			}

			void visitConst(pst::Access<pst::Const>) override { empty = true; }
		};

		struct HOUTFunctionMaker final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			// @TODO: make failure more explicit

			void visitFun(pst::Access<pst::Fun> stmt) final {
				// declaration:
				auto         decl = ctx.query<QueryDeclOfFun>(original_symbol);
				HOUTFunction output(decl);

				// body:
				auto fun_body = stmt->getBody();

				if (fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					// The `fun abc() = expr;` case.

					code::CodeBlock function_body
						= queryCodeOfSingleStmtFunctionBody(ctx, fun_body.unlock(ctx));
					output.body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				} else {
					CORE_ASSERT(
						fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
						"This should not happen"
					);
					code::CodeBlock function_body = queryCodeOfCodeBlock(ctx, fun_body);
					output.body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				}

				this->out.emplace(std::move(output));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function, "Function creation called on non-function symbol"
			);

			HOUTFunctionMaker func_maker(ctx, key);
			stmt(ctx, key).value()->acceptVisitor(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOFFun);
}
