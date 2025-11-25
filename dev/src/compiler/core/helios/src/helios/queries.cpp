#include "queries.hpp"

#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/simple.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/expression_type.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/except/exceptions.hpp>

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
						out.functions.push_back(ctx.query<QueryCodeOfFun>(sym));
					if (kind(sym) == SymbolKind::Class)
						appendClassConstructors(out.functions, sym, ctx);
				}
			}
			return out;
		}

		/**
		 * Append the constructors of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose constructors are to be appended.
		 * @param ctx The query context.
		 */
		static void appendClassConstructors(
			std::vector<HOUTFunction>& out_functions, const SymID class_sym, Context& ctx
		) {
			CORE_ASSERT(
				kind(class_sym) == SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			// For now, we handle only the class's primary constructor.
			// @TODO: #1290 Handle auxiliary constructors.

			const auto class_type
				= ctx.query<QueryTypeFromDefinition>(class_sym)
			          ->expect("Not handling errors here yet... (generating class constructor)")
			          .getType()
			          .as<tsh::ClassAbstractType>();
			const auto implicit_ctor
				= ctx.query<houtgen::QueryImplicitClassConstructor>(class_type);
			out_functions.push_back(*implicit_ctor);
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
					out.functions.push_back(ctx.query<QueryCodeOfFun>(sym));
			}

			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);

	struct IMPLEMENT_QUERY(QueryDeclOfFun, HOUTFunctionDeclaration) {
		struct DeclarationVisitor final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunctionDeclaration> out;

			DeclarationVisitor(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			void emplaceDeclaration(
				pst::AccessLocked<pst::ParamList>                  param_list,
				base::Optional<pst::AccessLocked<pst::ExprHolder>> ret
			) {
				// Default return type is a direct unit.
				tsh::SymbolType<> ret_type = tsh::SymbolType<>{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				if (ret.has_value()) {
					if (auto ctv
					    = ctx.query<QueryEvaluatePSTExpression>(ret.value().unlock(ctx)->getExpr()
					    )) {
						if (auto maybe_type = ctv.value().getType(ctx))
							ret_type = maybe_type.value();
						else
							return;
					} else {
						// We just fail here, because we can't continue without type.
						return;
					}
				}

				// Parameters:
				std::vector<code::Parameter> parameters;
				for (auto param: *param_list.unlock(ctx)) {
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

				HOUTFunctionDeclaration output(original_symbol, ret_type, std::move(parameters));

				this->out.emplace(std::move(output));
			}

			// @TODO: #1029 make failure more explicit
			void visitFun(pst::Access<pst::Fun> stmt) final {
				// @TODO: #1029 rest, flags, attributes, etc
				emplaceDeclaration(stmt->getParams(), stmt->getRet());
			}

			void visitFunDecl(pst::Access<pst::FunDecl> stmt) final {
				emplaceDeclaration(stmt->getParams(), stmt->getRet());
			}
		};

		static PResult getImplicitCtorDecl(
			Context& ctx, const houtgen::GeneratedSymbolData::ImplicitConstructor& ctor_data
		) {
			// Preamble
			using GeneratedSymbolData = houtgen::GeneratedSymbolData;
			using ImplicitConstructor = GeneratedSymbolData::ImplicitConstructor;
			using Parameter           = GeneratedSymbolData::Parameter;
			using std::ranges::to;
			using std::views::transform;

			// Get class data
			const auto class_type = ctx.query<QueryTypeFromDefinition>({ ctor_data.class_symbol })
			                            ->expect(
											"Not handling errors here yet... (getting "
											"declaration of generated constructor symbol)"
										)
			                            .getType()
			                            .as<tsh::ClassAbstractType>();

			const SymID class_symbol    = class_type.getSymbol();
			auto        class_interface = class_type.getInterface(ctx);

			const std::vector<tsh::InterfaceElement> fields
				= class_interface->getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			// Prepare the necessary symbols (of the constructor and its parameters).
			const SymID ctor_symbol        = ctx.query<houtgen::QueryGeneratedSymbol>({
					   .name                  = name(class_type.getSymbol()),
					   .generated_symbol_data = GeneratedSymbolData{ ImplicitConstructor{ class_symbol } },
            });
			const auto  result_symbol_type = tsh::SymbolType<>{
                class_type,
                tsh::ReferenceKind::Direct,
                tsh::Mutability::Mutable,
			};

			std::vector<code::Parameter> parameters;
			parameters.reserve(num_fields);

			u64 argument_index = 0;
			for (const auto& field: fields) {
				// Get the symbol of the constructor parameter corresponding to this field.
				const SymID argument_symbol = ctx.query<houtgen::QueryGeneratedSymbol>({
					.name = base::StrID(name(field.getSymbol())),
					.generated_symbol_data
					= GeneratedSymbolData{ Parameter{ ctor_symbol, argument_index } },
				});
				// Get the initial value for the field from the PST.
				const auto field_pst_data
					= symbolPst(field.getSymbol()).unlock(ctx).dynamicCast<pst::Field>().value();
				auto init_expr_opt = field_pst_data->getInit().map(
					[&](const pst::AccessLocked<pst::ExprHolder>& expr_holder) {
						return ctx
					        .query<QueryHoutOfExpr>(expr_holder.unlock(ctx)->getExpr().unlock(ctx))
					        .expect(
								"Not handling errors here yet..."
								"(getting field init expr for implicit ctor)"
							);
					}
				);
				auto init_expr_coerced_opt
					= std::move(init_expr_opt).map([&](Box<code::Expr>&& expr) {
						  const auto init_expr_type = expr->expression_type.getSymbolType();
						  const auto field_type     = field.getType(ctx);
						  const auto coercion
							  = canCoerce(ctx, init_expr_type, field_type)
					                .expect(base::strConcat(
										"Cannot coerce default field value of type ",
										init_expr_type.toString(),
										" to the field's expected type ",
										field_type.toString()
									));
						  return coercion.coerce(ctx, std::move(expr));
					  });

				// @TODO: #1328 Properly handle value categories / types (cont ref / ... / ...)
				// in class constructors.
				parameters.emplace_back(
					name(argument_symbol),
					field.getType(ctx),
					std::move(init_expr_coerced_opt),
					argument_symbol
				);
				argument_index++;
			}

			// Check that this logic did not diverge from `GeneratedSymbolData::getType()`.
			const auto expected_function_type = ctx.query<QueryTypeOfSymbol>({ ctor_symbol })
			                                        ->value()
			                                        .getType()
			                                        .as<tsh::FunctionAbstractType>();
			CORE_ASSERT(
				result_symbol_type == expected_function_type.getResultType(),
				"Generated constructor return type mismatch"
			);
			for (u64 i = 0; i < num_fields; i++) {
				CORE_ASSERT(
					parameters[i].type == expected_function_type.getParameterTypes().at(i),
					"Generated constructor parameter type mismatch"
				);
			}

			// Return the declaration.
			return HOUTFunctionDeclaration{
				ctor_symbol,
				result_symbol_type,
				std::move(parameters),
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (kind(key)) {
			case SymbolKind::Function:
			case SymbolKind::FunctionDeclaration: {
				variant_match(getSymRef(key)->other) {
					variant_case_novalue(PstSymbolData) {
						DeclarationVisitor decl_maker(ctx, key);
						stmt(ctx, key).value()->acceptVisitor(decl_maker);
						return std::move(decl_maker.out).value();
					}
					variant_case(houtgen::GeneratedSymbolData, generated_data) {
						variant_match(generated_data.data) {
							variant_case(
								houtgen::GeneratedSymbolData::ImplicitConstructor, ctor_data
							) return getImplicitCtorDecl(ctx, ctor_data);
							variant_default {
								// Other generated symbols are not functions.
								CORE_UNREACHABLE();
							}
						}
					}
					variant_default {
						// Builtin symbols are handled below due to a different SymbolKind.
						CORE_UNREACHABLE();
					}
				}
				CORE_UNREACHABLE();
			}
			case SymbolKind::BuiltinFunction: {
				const auto builtin_type = ctx.query<QueryTypeOfSymbol>({ key })
				                              ->expect(
												  "Not handling errors here yet... (getting "
												  "declaration of builtin function)"
											  )
				                              .getType()
				                              .as<tsh::FunctionAbstractType>();
				const auto return_type = builtin_type.getResultType();
				auto       parameters  = std::vector<code::Parameter>{};
				for (u32 i = 0; const auto& param_type: builtin_type.getParameterTypes()) {
					const auto param_symbol = ctx.query<houtgen::QueryGeneratedSymbol>({
						base::StrID(base::strConcat("_", i).c_str()),
						houtgen::GeneratedSymbolData{
							houtgen::GeneratedSymbolData::Parameter{
								.function_symbol = key,
								.parameter_index = i,
							},
						},
					});
					parameters.emplace_back(
						name(param_symbol), param_type, std::nullopt, param_symbol
					);
				}
				return HOUTFunctionDeclaration{
					key,
					return_type,
					std::move(parameters),
				};
			}
			default:
				CORE_UNREACHABLE();
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDeclOfFun);

	struct IMPLEMENT_QUERY(QueryCodeOfFun, HOUTFunction) {
		/**
		 * @brief Query extension to get hout CodeBlock from pst::CodeBlock or
		 * pst::CodeBlockOrStmt Might be changed into query in the future
		 */
		template<class Container>
		static auto queryCodeOfCodeBlock(query::Context& ctx, const Container& container) {
			code::CodeBlock block({});
			for (const auto& stmt: *container.unlock(ctx)) {
				HoutStmtMaker stmt_maker(ctx);
				stmt.unlock(ctx)->acceptVisitor(stmt_maker);
				if (stmt_maker.out.has_value())
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

			void visitAlias(pst::Access<pst::Alias>) override {}

			void visitUsing(pst::Access<pst::Using>) override {}

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

				// When this code was being written, this check could not be tested.
				// The optional result of this visitor is getting unwrapped without
				// checking for emptiness, which causes a panic.
				// @todo write a test for this once helios error handling is more robust
				auto location_mutability = location_type.getMutability();
				if (location_mutability == tsh::Mutability::Immutable) {
					ctx.log(
						makeBox<dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							assignment->getSourcePosition(),
							"Left side of assignment can't be immutable."
						)
					);
					return;  // fail
				}

				auto coercion
					= canCoerce(ctx, new_value_expr->expression_type.getSymbolType(), location_type);

				if (coercion.hasError()) {
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
				auto new_value_coerced = coercion.value().coerce(ctx, std::move(new_value_expr));

				output(code::AssignmentStmt(std::move(location_expr), std::move(new_value_coerced)));
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
				if (condition->expression_type.getType().getKind() != tsh::Kind::Bool) {
					auto coercion = canCoerce(
						ctx,
						condition->expression_type.getSymbolType(),
						tsh::SymbolType<>({ ctx.query<tsh::QueryBoolType>({}),
					                        tsh::ReferenceKind::Direct,
					                        tsh::Mutability::Mutable })
					);
					if (coercion.hasError())
						CORE_PANIC("Not handling errors here yet... (If condition coercion)");
					condition = coercion.value().coerce(ctx, std::move(condition));
				}

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
				auto symbol = ctx.query<QuerySymbolOfSTMT>(stmt);

				auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					"Handling errors is not supported in HOUT yet"
				);

				if (stmt->getValue().empty()) {
					// no initial value case

					// When this code was being written, this check could not be tested.
					// The optional result of this visitor is getting unwrapped without
					// checking for emptiness, which causes a panic.
					// @todo write a test for this once helios error handling is more robust
					if (symbol_type.getMutability() == tsh::Mutability::Immutable) {
						ctx.log(makeBox<
								dia::PlaceholderMessage<dia::Error, dia::Message::Domain::TypeCheck>>(
							stmt->getSourcePosition(),
							base::strConcat("Immutable variables must have an initial value")
						));
						return;  // fail
					}

					throw base::NotYetImplemented(
						"Variable declarations without initial value are not supported in HOUT yet."
						" We should add default initialization here."
					);
				} else {
					auto initial_value
						= ctx.query<QueryHoutOfExpr>(stmt->getValue().value().unlock(ctx)->getExpr())
					          .expect("Not handling errors here yet... (variable initial value)");
					// used for error reporting:
					auto initial_value_type = initial_value->expression_type.getSymbolType();
					auto coercion           = canCoerce(
                        ctx, initial_value->expression_type.getSymbolType(), symbol_type
                    );
					if (coercion.hasError()) {
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
						coercion.value().coerce(ctx, std::move(initial_value)), symbol_type, symbol
					));
				}
			}

			void visitConst(pst::Access<pst::Const>) override {
				CORE_PANIC("Const stmt in function body not supported in HOUT yet\n");
			}
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
				auto decl = ctx.query<QueryDeclOfFun>(original_symbol);

				// body:
				std::shared_ptr<const code::CodeBlock> output_body = nullptr;
				auto                                   fun_body    = stmt->getBody();

				if (fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					// The `fun abc() = expr;` case.

					code::CodeBlock function_body
						= queryCodeOfSingleStmtFunctionBody(ctx, fun_body.unlock(ctx));
					output_body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				} else {
					CORE_ASSERT(
						fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
						"This should not happen"
					);
					code::CodeBlock function_body = queryCodeOfCodeBlock(ctx, fun_body);
					output_body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				}
				CORE_ASSERT(output_body != nullptr, "Function declaration must be present here");

				this->out.emplace(HOUTFunction{ decl, output_body });
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

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOfFun);
}
