#include "queries.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/expressions/assignment.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/expressions/coercions.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/hout_code_generation/hout_stmt_compilation.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios_private/utils/pst_walkers.hpp>
#include <typesystem/higher/expression_type.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/symbol_type.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	/**
	 * @brief Query function return type, deduced based on return statements in its body.
	 */
	DECLARE_QUERY(QueryReturnTypeDeduction, SymID, CRef<query::QResult<tsh::SymbolType<>>>, ({}))

	struct IMPLEMENT_QUERY(QueryModuleHOUT, query::QResult<HOUTUnit>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// We want to continue gathering other entities
			// even if some function queries fail,
			// so we store in this variable whether any failure occurred,
			// and return failure at the end if so.
			bool is_failed = false;

			MCRef<std::vector<ScopeID>> scopes_to_process;

			auto scopes_in_module = ctx.query<QueryScopesInModule>(key);
			variant_match(scopes_in_module->value) {
				variant_case(QueryScopesInModuleValue::Success, success) {
					scopes_to_process = &success.scopes;
				}
				variant_case(QueryScopesInModuleValue::Failure, failure) {
					scopes_to_process = &failure.partial_scopes;
					is_failed
						= true;  // we mark the whole query as failed, even if we have some scopes
				}
				variant_default { CORE_UNREACHABLE(); }
			}

			HOUTUnit out;

			std::vector<query::TaskHandle> scheduled_tasks;
			std::vector<SymID>             class_symbols;

			for (auto scope: *scopes_to_process) {
				auto symbols_in_scope = ctx.query<QuerySymbolsInScope>(scope);

				for (auto sym: *symbols_in_scope) {
					// grab constants:
					if (kind(sym) == SymbolKind::Const)
						out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Constant);
					if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
						out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Variable);

					// grab functions:
					if (kind(sym) == SymbolKind::Function)
						scheduled_tasks.emplace_back(ctx.schedule<QueryCodeOfFun>(sym));
					if (kind(sym) == SymbolKind::Class) class_symbols.emplace_back(sym);
				}
			}

			for (auto class_sym: class_symbols) {
				// we postpone this past function scheduling, as
				// appendClassConstructors may be time consuming.
				appendClassConstructors(out.functions, class_sym, ctx);
				if (appendClassMethodsWithFail(out.functions, class_sym, ctx)) {
					is_failed = true;
					continue;
				}
			}

			for (auto handler: scheduled_tasks) {
				// we "catch" failure here to continue gathering other functions:
				auto hout_function = ctx.await<QueryCodeOfFun>(handler);
				if (hout_function->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					out.functions.emplace_back(&hout_function->valueOrPanic());
				}
			}

			if (is_failed) return query::Failed();

			return out;
		}

		/**
		 * Append the constructors of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose constructors are to be appended.
		 * @param ctx The query context.
		 */
		static void appendClassConstructors(
			std::vector<CRef<HOUTFunction>>& out_functions, const SymID class_sym, Context& ctx
		) {
			CORE_ASSERT(
				kind(class_sym) == SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			// For now, we handle only the class's primary constructor.
			// @TODO: #1290 Handle auxiliary constructors.

			const auto class_type = ctx.query<QueryTypeFromDefinition>(class_sym)
			                            ->valueOrThrow()
			                            .getType()
			                            .as<tsh::ClassAbstractType>();
			const auto& implicit_ctor
				= ctx.query<houtgen::QueryImplicitClassConstructor>(class_type)->valueOrThrow();
			out_functions.emplace_back(&implicit_ctor);
		}

		/**
		 * Append the methods of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose methods are to be appended.
		 * @param ctx The query context.
		 *
		 * @return Whether any method queries failed.
		 */
		static bool appendClassMethodsWithFail(
			std::vector<CRef<HOUTFunction>>& out_functions, const SymID class_sym, Context& ctx
		) {
			CORE_ASSERT(
				kind(class_sym) == SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			const auto class_type = ctx.query<QueryTypeFromDefinition>(class_sym)
			                            ->valueOrThrow()
			                            .getType()
			                            .as<tsh::ClassAbstractType>();

			auto methods = class_type.getInterface(ctx)->getMethodsView();


			bool is_failed = false;

			for (const auto& method: methods) {
				auto method_sym  = method.getSymbol();
				auto hout_method = ctx.query<QueryCodeOfFun>(method_sym);
				if (hout_method->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					out_functions.emplace_back(&hout_method->valueOrPanic());
				}
			}
			return is_failed;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUT);

	struct IMPLEMENT_QUERY(QueryModuleHOUTRecursively, query::QResult<std::vector<CRef<HOUTUnit>>>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			std::vector<CRef<HOUTUnit>> out = { &ctx.query<QueryModuleHOUT>(key)->valueOrThrow() };

			auto submodules = ctx.query<frontend::QuerySubmodules>(key);
			for (auto submodule: *submodules) {
				// @TODO optimize multiple concatenations
				auto submodule_hout
					= ctx.query<QueryModuleHOUTRecursively>(submodule.value).valueOrThrow();
				for (const auto& i: submodule_hout) out.push_back(i);
			}
			return out;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUTRecursively);

	struct IMPLEMENT_QUERY(QueryTopLevelEntities, query::QResult<HOUTUnit>) {
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
					out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Constant);
				if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
					out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Variable);
				// grab functions:
				if (kind(sym) == SymbolKind::Function)
					out.functions.emplace_back(&ctx.query<QueryCodeOfFun>(sym)->valueOrThrow());
			}

			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);

	struct IMPLEMENT_QUERY(QueryReturnTypeDeduction, query::QResult<tsh::SymbolType<>>) {
		struct ReturnTypeCollector final: public pst::PstVisitorEmpty {
			query::Context& ctx;
			SymID           original_symbol;

			/**
			 * Variable used to distinguish between initial invocation of the visitor on the whole
			 * function body, and recursive invocations on nested functions.
			 */
			bool initial_invocation;

			std::set<tsh::SymbolType<>> out;

			ReturnTypeCollector(query::Context& ctx, SymID symbol, bool initial_invocation = false):
				  ctx(ctx),
				  original_symbol(symbol),
				  initial_invocation(initial_invocation) {}

			// @TODO: #1710 visits for all valid stmt-s

			template<class T>
			void output(T&& value) {
				this->out.emplace(std::forward<T>(value));
			}

			template<class Stmts>
			void visitRecursion(const Stmts& stmts) {
				for (const auto& stmt: getStmtsFromStmtAggregate(ctx, stmts))
					stmt.unlock(ctx)->acceptVisitor(*this);
			}

			template<class FuncLike>
			void visitFuncLike(const FuncLike& func_like) {
				if (not initial_invocation) {
					// we are visiting a nested function, so we should not collect return types from it
					return;
				}
				// the later uses of this visitor should know that they are visiting a nested
				// function, so we set this variable to false.
				// We can keep it set to false, since we fill be here in the top level function only
				// once.
				initial_invocation = false;

				auto fun_body = func_like->getBody();

				if (fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					auto as_expr = fun_body.unlock(ctx)
					                   ->getStmt()
					                   .unlock(ctx)
					                   .template dynamicCast<pst::ExprStmt>();
					if (as_expr) {
						auto expr = ctx.query<QueryHoutOfExpr>(
										   as_expr.value()->getExpr().unlock(ctx)->getExpr()
						)
						                ->valueOrThrow()
						                .ref();
						output(expr->expression_type.getSymbolType());
					} else {
						CORE_PANIC(
							"Function body in single-statement function must be an expression stmt"
						);
					}
				} else {
					CORE_ASSERT(
						fun_body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
						"This should not happen"
					);

					for (const auto& stmt: getStmtsFromStmtAggregate(ctx, fun_body))
						stmt.unlock(ctx)->acceptVisitor(*this);
				}
			}

			void visitFun(pst::Access<pst::Fun> fun) final { visitFuncLike(fun); }

			void visitMethod(pst::Access<pst::Method> method) final { visitFuncLike(method); }

			void visitReturn(pst::Access<pst::Return> stmt) final {
				if (auto val = stmt->getValue()) {
					auto expr = ctx.query<QueryHoutOfExpr>(val.value().unlock(ctx)->getExpr())
					                ->valueOrThrow()
					                .ref();
					output(expr->expression_type.getSymbolType());
				} else {
					// "void" return should actually deduce to unit type:
					output(tsh::SymbolType<>{
						tsh::getUnitType(),
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Mutable,
					});
				}
			}

			void visitIf(pst::Access<pst::If> stmt) final {
				visitRecursion(stmt->getThenBody());

				if (stmt->getElseBody().has_value()) visitRecursion(stmt->getElseBody().value());
			}

			void visitWhile(pst::Access<pst::While> stmt) final { visitRecursion(stmt->getBody()); }
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			ReturnTypeCollector return_collector(ctx, key, true);
			auto                fun = stmt(ctx, key).value();
			fun->acceptVisitor(return_collector);
			switch (return_collector.out.size()) {
			case 0:
				// there are no returns to deduce the type
				// we default to unit type
				return tsh::SymbolType<>{
					tsh::getUnitType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			case 1:
				// deduced type is conclusive
				return *return_collector.out.begin();
			default:
				// there are multiple candidates and return type deduction is inconclusive
				ctx.logInt(makeBox<dia_int::PlaceholderCodeError>(
					"Function declared with no explicit return type and inconsistent return "
					"statements.",
					fun->getSourcePosition()
				));
				return query::Failed();
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryReturnTypeDeduction);

	struct IMPLEMENT_QUERY(QueryDeclOfFun, query::QResult<HOUTFunctionDeclaration>) {
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
				auto ret_type = tsh::SymbolType<>{
					tsh::getUnitType(),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
				code::ElementOrigin origin = code::generatedOrigin();

				// Set return type if provided.
				if (ret.has_value()) {
					const auto ret_type_ctv
						= getTypeCTVFromPST(ctx, ret.value().unlock(ctx)->getExpr()).valueOrThrow();
					ret_type = ret_type_ctv.get<tsh::SymbolType<>>().value();
					origin   = code::multiplePstOrigin({ param_list.unlock(ctx),
					                                     ret.value().unlock(ctx) });
				}
				// Deduce return type if not provided.
				else {
					ret_type = ctx.query<QueryReturnTypeDeduction>(original_symbol)->valueOrThrow();
					origin   = code::pstOrigin(param_list.unlock(ctx));
				}

				// Parameters:
				std::vector<code::Parameter> parameters;
				for (auto param: *param_list.unlock(ctx)) {
					auto  param_symbol = ctx.query<QuerySymbolOfSTMT>({ param }).valueOrThrow();
					auto  param_name   = name(param_symbol);
					auto& param_type
						= ctx.query<QueryTypeOfSymbol>({ param_symbol })->valueOrThrow();

					auto value = param.unlock(ctx)->getValue();
					if (value.empty()) {
						parameters.emplace_back(
							param_name,
							param_type,
							std::nullopt,
							param_symbol,
							code::pstOrigin(param.unlock(ctx))
						);
					} else {
						auto initial_value
							= getHoutOfExprWithExpectedType(
								  ctx, value.value().unlock(ctx)->getExpr(), param_type
							)
						          .valueOrThrow();

						parameters.emplace_back(
							param_name,
							param_type,
							std::move(initial_value),
							param_symbol,
							code::pstOrigin(param.unlock(ctx))
						);
					}
				}

				HOUTFunctionDeclaration output(
					original_symbol, ret_type, std::move(parameters), origin
				);

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

			void visitMethod(pst::Access<pst::Method> stmt) final {
				emplaceDeclaration(stmt->getParams(), stmt->getRet());

				const auto  self_scope  = ctx.query<QueryPrimaryCodeScopeFor>(stmt);
				const SymID self_symbol = ctx.query<houtgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= houtgen::GeneratedSymbolData{ houtgen::GeneratedSymbolData::SelfParameter{
						.method_symbol = this->original_symbol, .scope = self_scope } },
				});

				this->out->parameters.insert(
					this->out->parameters.begin(),
					code::Parameter{ .name = name(self_symbol),
				                     .type
				                     = ctx.query<QueryTypeOfSymbol>(self_symbol)->valueOrThrow(),
				                     .initial_value = std::nullopt,
				                     .helios_symbol = self_symbol,
				                     .origin        = code::generatedOrigin() }
				);
			}
		};

		/**
		 * @brief Get the declaration of an implicit class constructor.
		 */
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
			                            ->valueOrThrow()
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
				auto init_expr_opt         = field_pst_data->getInit();
				auto init_expr_coerced_opt = init_expr_opt.map(
					[&](pst::AccessLocked<pst::ExprHolder> expr_holder) -> Box<code::Expr> {
						const auto field_type = field.getType(ctx);
						auto       expr
							= getHoutOfExprWithExpectedType(
								  ctx, expr_holder.unlock(ctx)->getExpr().unlock(ctx), field_type
							)
					              .valueOrThrow();
						return expr;
					}
				);

				// @TODO: #1328 Properly handle value categories / types (cont ref / ... / ...)
				// in class constructors.
				parameters.emplace_back(
					name(argument_symbol),
					field.getType(ctx),
					std::move(init_expr_coerced_opt),
					argument_symbol,
					code::pstOrigin(field_pst_data).generatedFrom()
				);
				argument_index++;
			}

			// Check that this logic did not diverge from `GeneratedSymbolData::getType()`.
			const auto expected_function_type = ctx.query<QueryTypeOfSymbol>({ ctor_symbol })
			                                        ->valueOrThrow()
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
				ctor_symbol, result_symbol_type, std::move(parameters), code::generatedOrigin()
			};
		}

		/**
		 * @brief Get the declaration of a builtin function, or one which does not have its
		 * parameters specified anywhere. The parameter symbols are set as compiler-generated.
		 */
		static PResult getBuiltinDecl(Context& ctx, QKey fun) {
			const auto builtin_type = ctx.query<QueryTypeOfSymbol>({ fun })
			                              ->valueOrThrow()
			                              .getType()
			                              .as<tsh::FunctionAbstractType>();
			const auto return_type = builtin_type.getResultType();
			auto       parameters  = std::vector<code::Parameter>{};
			for (u32 i = 0; const auto& param_type: builtin_type.getParameterTypes()) {
				const auto param_symbol = ctx.query<houtgen::QueryGeneratedSymbol>({
					base::StrID(base::strConcat("_", i).c_str()),
					houtgen::GeneratedSymbolData{
						houtgen::GeneratedSymbolData::Parameter{
							.function_symbol = fun,
							.parameter_index = i,
						},
					},
				});
				parameters.emplace_back(
					name(param_symbol),
					param_type,
					std::nullopt,
					param_symbol,
					code::generatedOrigin()
				);
			}
			return HOUTFunctionDeclaration{
				fun,
				return_type,
				std::move(parameters),
				code::generatedOrigin(),
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (kind(key)) {
			case SymbolKind::Function:
			case SymbolKind::FunctionDeclaration:
			case SymbolKind::Method: {
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
							) {
								return getImplicitCtorDecl(ctx, ctor_data);
							}
							variant_case_novalue(houtgen::GeneratedSymbolData::BuiltinOperator) {
								return getBuiltinDecl(ctx, key);
							}
							variant_default {
								// Other generated symbols are not functions.
								CORE_UNREACHABLE();
							}
						}
					}
					variant_case(builtin::BuiltinFunctionData, builtin) {
						return getBuiltinDecl(ctx, key);
					}
					variant_default { CORE_UNREACHABLE(); }
				}
				CORE_UNREACHABLE();
			}
			default:
				CORE_UNREACHABLE();
			}
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryDeclOfFun);

	struct IMPLEMENT_QUERY(QueryCodeOfFun, query::QResult<HOUTFunction>) {
		/**
		 * Creates HOUT code block from single-statement function body.
		 * i.e.: handles the `fun abc() = expr;` case.
		 * Should only be used for the whole function body.
		 */
		static code::CodeBlock queryCodeOfSingleStmtFunctionBody(
			query::Context&                   ctx,
			pst::Access<pst::CodeBlockOrStmt> body,
			tsh::SymbolType<>                 return_type
		) {
			CORE_ASSERT(body->getType() == pst::CodeBlockOrStmt::Type::SingleStmt, "Bad body type!");

			code::CodeBlock block({});

			auto stmt    = body->getStmt().unlock(ctx);
			auto as_expr = stmt.dynamicCast<pst::ExprStmt>();
			if (as_expr) {
				auto expr_coerced
					= getHoutOfExprWithExpectedType(
						  ctx, as_expr.value()->getExpr().unlock(ctx)->getExpr(), return_type
					)
				          .valueOrThrow();

				block.statements.emplace_back(makeBox<code::ReturnStmt>(
					expr_coerced->origin.generatedFrom(), std::move(expr_coerced)
				));
				return block;
			} else {
				ctx.logInt(makeBox<SingleStmtFunctionMustBeExprError>(stmt->getSourcePosition()));
				CORE_PANIC("Not handling errors here yet... (single stmt function body)");
			}
		}

		struct HOUTFunctionMaker final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			std::shared_ptr<const code::CodeBlock> processBody(
				const HOUTFunctionDeclaration& decl, pst::AccessLocked<pst::CodeBlockOrStmt> body
			) {
				std::shared_ptr<const code::CodeBlock> output_body = nullptr;

				if (body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::SingleStmt) {
					// The `fun abc() = expr;` case.

					code::CodeBlock function_body
						= queryCodeOfSingleStmtFunctionBody(ctx, body.unlock(ctx), decl.return_type);
					output_body = std::make_shared<const code::CodeBlock>(std::move(function_body));
				} else {
					CORE_ASSERT(
						body.unlock(ctx)->getType() == pst::CodeBlockOrStmt::Type::CodeBlock,
						"This should not happen"
					);
					output_body = houtgen::compileCodeOfCodeBlock(ctx, body, decl.return_type);
				}
				CORE_ASSERT(output_body != nullptr, "Function declaration must be present here");

				return output_body;
			}

			void visitFun(pst::Access<pst::Fun> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();

				// body:
				auto fun_body    = stmt->getBody();
				auto output_body = processBody(decl, fun_body);

				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}

			void visitMethod(pst::Access<pst::Method> stmt) final {
				// declaration:
				auto& decl = ctx.query<QueryDeclOfFun>(original_symbol)->valueOrThrow();

				// body:
				auto fun_body    = stmt->getBody();
				auto output_body = processBody(decl, fun_body);

				this->out.emplace(HOUTFunction(code::pstOrigin(stmt), &decl, output_body));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function or kind(key) == SymbolKind::Method,
				"Function creation called on non-function and non-method symbol"
			);
			CORE_ASSERT(
				getSymRef(key)->getPSTDataOpt().has_value(),
				"Query code of function does not support generated functions"
			);
			HOUTFunctionMaker func_maker(ctx, key);
			stmt(ctx, key).value()->acceptVisitor(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOfFun);
}
