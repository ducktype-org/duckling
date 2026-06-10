#include "function_queries.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/code_block_or_statement.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expr_stmt.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {
	/**
	 * @brief Query function return type, deduced based on return statements in its body.
	 */
	DECLARE_QUERY(QueryReturnTypeDeduction, SymID, CRef<query::QResult<tsh::SymbolType<>>>, ({}))

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

			void visitFor(pst::Access<pst::For> stmt) final { visitRecursion(stmt->getBody()); }
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
				ctx.logInt(makeBox<dia_int::PlaceholderError>(
					"Function declared with no explicit return type and inconsistent return "
					"statements.",
					fun->getStablePosition()
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
					origin   = code::multiplePstOriginOrdered({ param_list.unlock(ctx),
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
				const SymID self_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= defgen::GeneratedSymbolData{ defgen::GeneratedSymbolData::SelfParameter{
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
			Context& ctx, const defgen::GeneratedSymbolData::ImplicitConstructor& ctor_data
		) {
			// Preamble
			using GeneratedSymbolData = defgen::GeneratedSymbolData;
			using ImplicitConstructor = GeneratedSymbolData::ImplicitConstructor;
			using Parameter           = GeneratedSymbolData::Parameter;
			using std::ranges::to;
			using std::views::transform;

			// Get type data
			const auto target_type = ctor_data.target_type;
			auto       interface   = target_type.getInterface(ctx);

			const std::vector<tsh::InterfaceElement> fields
				= interface->getFieldsView() | to<std::vector>();
			const u64 num_fields = fields.size();

			base::StrID ctor_name;
			switch (target_type.getKind()) {
			case tsh::Kind::Class:
				ctor_name = name(target_type.as<tsh::ClassAbstractType>().getSymbol());
				break;
			case tsh::Kind::Tuple:
				ctor_name = ctx
				                .query<mangler::QueryMangledType>(tsh::SymbolType<>::withDefaults(
									target_type.as<tsh::TupleAbstractType>()
								))
				                ->valueOrThrow();
				break;
			default:
				CORE_PANIC("Implicit ctor of type kind", target_type.getKind(), " is not handled.");
			}

			// Prepare the necessary symbols (of the constructor and its parameters).
			const SymID ctor_symbol        = ctx.query<defgen::QueryGeneratedSymbol>({
					   .name                  = ctor_name,
					   .generated_symbol_data = GeneratedSymbolData{ ImplicitConstructor{ target_type } },
            });
			const auto  result_symbol_type = tsh::SymbolType<>{
                target_type,
                tsh::ReferenceKind::Direct,
                tsh::Mutability::Mutable,
			};

			std::vector<code::Parameter> parameters;
			parameters.reserve(num_fields);

			u64 argument_index = 0;
			for (const auto& field: fields) {
				// Get the symbol of the constructor parameter corresponding to this field.
				const SymID argument_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID(name(field.getSymbol())),
					.generated_symbol_data
					= GeneratedSymbolData{ Parameter{ ctor_symbol, argument_index } },
				});

				base::Optional<BoxOrCRef<code::Expr>> init_expr_coerced_opt = std::nullopt;
				code::ElementOrigin                   field_origin = code::generatedOrigin();
				if (maybeSymbolPst(field.getSymbol()).has_value()) {
					// Get the initial value for the field from the PST.
					const auto field_pst_data = maybeSymbolPst(field.getSymbol())
					                                .value()
					                                .unlock(ctx)
					                                .dynamicCast<pst::Field>()
					                                .value();
					field_origin          = code::pstOrigin(field_pst_data).generatedFrom();
					auto init_expr_opt    = field_pst_data->getInit();
					init_expr_coerced_opt = init_expr_opt.map(
						[&](pst::AccessLocked<pst::ExprHolder> expr_holder
					    ) -> BoxOrCRef<code::Expr> {
							const auto field_type = field.getType(ctx);
							auto       expr
								= getHoutOfExprWithExpectedType(
									  ctx, expr_holder.unlock(ctx)->getExpr().unlock(ctx), field_type
								)
						              .valueOrThrow();
							return expr;
						}
					);
				}
				// @TODO: #1328 Properly handle value categories / types (cont ref / ... / ...)
				// in class constructors.
				parameters.emplace_back(
					name(argument_symbol),
					field.getType(ctx),
					std::move(init_expr_coerced_opt),
					argument_symbol,
					field_origin
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
		 * @brief Get the declaration of a function-like symbol based on its type.
		 */
		static PResult funDeclFromType(Context& ctx, QKey fun) {
			const auto builtin_type = ctx.query<QueryTypeOfSymbol>({ fun })
			                              ->valueOrThrow()
			                              .getType()
			                              .as<tsh::FunctionAbstractType>();
			const auto return_type = builtin_type.getResultType();
			auto       parameters  = std::vector<code::Parameter>{};
			for (u32 i = 0; const auto& param_type: builtin_type.getParameterTypes()) {
				const auto param_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					base::StrID(base::strConcat("_", i).c_str()),
					defgen::GeneratedSymbolData{
						defgen::GeneratedSymbolData::Parameter{
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
				i++;
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
					variant_case(defgen::GeneratedSymbolData, generated_data) {
						variant_match(generated_data.data) {
							variant_case(
								defgen::GeneratedSymbolData::ImplicitConstructor, ctor_data
							) {
								return getImplicitCtorDecl(ctx, ctor_data);
							}
							variant_case(
								defgen::GeneratedSymbolData::DefaultClassConstructor, ctor_data
							) {
								const auto class_type
									= ctx.query<QueryTypeFromDefinition>({ ctor_data.class_symbol })
								          ->valueOrThrow()
								          .getType()
								          .as<tsh::ClassAbstractType>();

								const auto return_type = tsh::SymbolType<>{
									class_type, tsh::ReferenceKind::Direct, tsh::Mutability::Mutable
								};

								return HOUTFunctionDeclaration{
									key, return_type, {}, code::generatedOrigin()
								};
							}
							variant_case(
								defgen::GeneratedSymbolData::DefaultTupleConstructor, ctor_data
							) {
								const auto return_type
									= tsh::SymbolType<>{ ctor_data.tuple_type,
									                     tsh::ReferenceKind::Direct,
									                     tsh::Mutability::Mutable };

								return HOUTFunctionDeclaration{
									key, return_type, {}, code::generatedOrigin()
								};
							}
							variant_case(
								defgen::GeneratedSymbolData::DefaultStaticArrayConstructor, ctor_data
							) {
								const auto return_type
									= tsh::SymbolType<>{ ctor_data.array_type,
									                     tsh::ReferenceKind::Direct,
									                     tsh::Mutability::Mutable };

								return HOUTFunctionDeclaration{
									key, return_type, {}, code::generatedOrigin()
								};
							}
							variant_case(
								defgen::GeneratedSymbolData::ToStringMethod, to_string_data
							) {
								return funDeclFromType(ctx, key);
							}
							variant_case(defgen::GeneratedSymbolData::DefaultDestructor, dtor_data) {
								return funDeclFromType(ctx, key);
							}
							variant_case(defgen::GeneratedSymbolData::LengthMethod, length_method) {
								return funDeclFromType(ctx, key);
							}
							variant_case_novalue(defgen::GeneratedSymbolData::BuiltinOperator) {
								return funDeclFromType(ctx, key);
							}
							variant_case_novalue(
								defgen::GeneratedSymbolData::ReplExpressionWrapper,
								defgen::GeneratedSymbolData::ReplInstructionWrapper
							) {
								return funDeclFromType(ctx, key);
							}
							variant_case_novalue(defgen::GeneratedSymbolData::ScriptMainWrapper) {
								// Script main is a generated symbol with a regular function
								// signature, so it needs a normal HOUT declaration for the backend
								// pipeline.
								return funDeclFromType(ctx, key);
							}
							variant_default {
								// Other generated symbols are not functions.
								CORE_UNREACHABLE();
							}
						}
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
}
