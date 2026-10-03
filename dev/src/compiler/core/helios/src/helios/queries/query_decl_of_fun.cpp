#include "function_queries.hpp"

#include <frontend/pst_parser/elements/hierarchy/actions/all_actions.hpp>
#include <frontend/pst_parser/elements/hierarchy/actions/return.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/copy_constructor.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/destructor.hpp>
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
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/tsh/expression_type.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/main_return_type.hpp>
#include <helios_private/attributes/backend_dependent.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/hout_creation/hout_stmt_compilation.hpp>
#include <helios_private/pst_layer/stmts_from_aggregate.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/generated_symbol_data.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <diagnostic/placeholder.hpp>
#include <lexer/token_common.hpp>
#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>

#include <algorithm>
#include <set>

namespace compiler::helios {
	namespace {
		/**
		 * @brief Whether every path through @p stmts ends in a `return`.
		 * See EveryPathReturnsVisitor for what this does and does not recognise.
		 */
		template<class Stmts>
		bool everyPathReturns(query::Context& ctx, const Stmts& stmts);

		/**
		 * @brief Whether every path through one statement ends in a `return`.
		 *
		 * This is conservative and purely structural. If there isn't an obvious return on every
		 * path, it answers `false`. Proper reachability and dead code analysis live in MIR.
		 */
		class EveryPathReturnsVisitor final: public pst::PstVisitorEmpty {
		public:
			query::Context& ctx;

			/**
			 * @brief The answer for the visited statement. False until something proves otherwise.
			 */
			bool every_path_returns = false;

			explicit EveryPathReturnsVisitor(query::Context& ctx): ctx(ctx) {}

			void visitReturn(pst::Access<pst::Return>) final { every_path_returns = true; }

			void visitIf(pst::Access<pst::If> stmt) final {
				if (stmt->isConst()) {
					// Only the taken branch is ever compiled, so only it has a say.
					auto condition_holder = stmt->getCondition();
					if (!condition_holder.has_value()) query::throwFailed();

					const auto taken
						= getBoolCTVFromPST(ctx, condition_holder.value().unlock(ctx)->getExpr())
					          .valueOrThrow();

					if (taken)
						every_path_returns = everyPathReturns(ctx, stmt->getThenBody());
					else if (stmt->getElseBody().has_value())
						every_path_returns = everyPathReturns(ctx, stmt->getElseBody().value());

					return;
				}

				// Without an `else` the condition may be false and the body skipped entirely, so
				// there is always a path around this statement.
				if (!stmt->getElseBody().has_value()) return;

				every_path_returns = everyPathReturns(ctx, stmt->getThenBody())
				                  && everyPathReturns(ctx, stmt->getElseBody().value());
			}
		};

		template<class StmtsAggregate>
		bool everyPathReturns(query::Context& ctx, const StmtsAggregate& stmts) {
			// A statement that always returns ends the block, so whatever follows is ignored.
			for (const auto& stmt: getStmtsFromStmtAggregate(ctx, stmts)) {
				EveryPathReturnsVisitor visitor(ctx);
				stmt.unlock(ctx)->acceptVisitor(visitor);
				if (visitor.every_path_returns) return true;
			}
			return false;
		}

		/**
		 * @brief Whether a deduction candidate is the uninhabited `void` type.
		 */
		bool isVoid(const tsh::SymbolType<>& type) {
			return type.getType().getKind() == tsh::Kind::Void;
		}
	}

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

			/**
			 * @brief Whether every path through the function body ends in a `return`, i.e.
			 * whether the body cannot fall off its end and return implicitly.
			 */
			bool body_always_returns = false;

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

						// The body expression is the returned value, so there is no way around it.
						body_always_returns = true;
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

					body_always_returns = everyPathReturns(ctx, fun_body);

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
				if (stmt->isConst()) {
					// Only the taken branch takes part in the return type deduction, the other
					// one is never compiled.
					auto condition_holder = stmt->getCondition();
					if (!condition_holder.has_value()) query::throwFailed();

					auto taken
						= getBoolCTVFromPST(ctx, condition_holder.value().unlock(ctx)->getExpr())
					          .valueOrThrow();

					if (taken)
						visitRecursion(stmt->getThenBody());
					else if (stmt->getElseBody().has_value())
						visitRecursion(stmt->getElseBody().value());

					return;
				}

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

			// First, we check if the function never returns, i.e. it always returns `void`
			// and there is no path to the end of the function's body.
			// Note: returning `void` does not *actually* ever happen, because creating
			// a `void` value is not possible. However, the typesystem handles it well.
			const bool never_returns = !return_collector.out.empty()
			                        && return_collector.body_always_returns
			                        && std::ranges::all_of(return_collector.out, isVoid);

			// If there exists a non-void return possibility, we ignore all void return types for
			// return type deduction. In other words, `void` submits to all other types.
			// Otherwise, only `void` return types exist, and we keep them for the deduction.
			if (!never_returns) std::erase_if(return_collector.out, isVoid);

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
				// deduced type is conclusive (albeit there might still be a fallthrough to the
				// end of the function, but it then does not contribute to return type deduction).
				return *return_collector.out.begin();
			default:
				// there are multiple candidates and return type deduction is inconclusive
				ctx.logInt(makeBox<dia::PlaceholderError>(
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

	/**
	 * This function verifies if the applied attributes are semantically correct
	 * on the function. For example we check if the BackendDependent attribute has
	 * some implementations.
	 * This is needed, because some attributes can have additional requirements on the function
	 * declaration and the surrounding code.
	 */
	static void verifyFunctionAttributes(
		query::Context& ctx, pst::Access<pst::Stmt> pst_stmt, const HOUTFunctionDeclaration& fun_decl
	) {
		if (pst_stmt->getAttributes().empty()) return;

		if (hasAttribute<attributes::DVMOnlyImpl>(fun_decl.original_symbol)
		    or hasAttribute<attributes::NativeOnlyImpl>(fun_decl.original_symbol)) {
			verifyBackendImplAttrUsage(ctx, fun_decl);
			return;
		}
		if (hasAttribute<attributes::BackendDependent>(fun_decl.original_symbol)) {
			verifyBackendDependentAttrUsage(ctx, fun_decl);
			return;
		}
	}

	/**
	 * @brief Deduces the operatoriness of a user-declared function/method from its name and arity
	 * (parameter count, including an implicit `self` for methods).
	 *
	 * @note There is no dedicated syntax yet to declare fixity (prefix vs. suffix), so a
	 * single-parameter operator name is assumed to be a prefix operator. Suffix stays unreachable
	 * from user code until that syntax exists.
	 * @TODO: #3131 Extract fixity in unary operators from keywords used in PST.
	 */
	static HOUTFunctionDeclaration::Operatoriness operatorinessFromNameAndArity(
		base::StrID name, u64 arity
	) {
		using Operatoriness = HOUTFunctionDeclaration::Operatoriness;

		if (!lexer::isOperatorSymbolString(name.strView())) return Operatoriness::None;

		switch (arity) {
		case 1:
			return Operatoriness::Prefix;
		case 2:
			return Operatoriness::Infix;
		default:
			return Operatoriness::None;
		}
	}

	static bool isValidMainReturnType(query::Context& ctx, const tsh::SymbolType<>& return_type) {
		const auto required_type = requiredMainReturnType(ctx);

		return return_type.getType() == required_type.getType()
		    && return_type.getRefKind() == required_type.getRefKind();
	}

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
				base::Optional<pst::AccessLocked<pst::ExprHolder>> ret,
				HOUTFunctionDeclaration::Operatoriness             operatoriness
			) {
				const bool global_main_definition = isGlobalMain(original_symbol)
				                                 && kind(original_symbol) == SymbolKind::Function;

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

					if (global_main_definition && not isValidMainReturnType(ctx, ret_type)) {
						ctx.logInt(makeBox<InvalidMainReturnTypeError>(
							ret.value().unlock(ctx)->getStablePosition(),
							makeBox<InteractiveType>(ctx, ret_type)
						));
						query::throwFailed();
					}

					origin = code::multiplePstOriginOrdered({
						param_list.unlock(ctx),
						ret.value().unlock(ctx),
					});
				}
				// A global main without an explicit return type is treated as `main -> i64`.
				else if (global_main_definition) {
					ret_type = requiredMainReturnType(ctx);
					origin   = code::pstOrigin(param_list.unlock(ctx));
				}
				// Deduce return type for ordinary functions.
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

				auto abi_wrapped = ctx.query<QuerySymbolABI>(original_symbol)->valueOrThrow();

				HOUTFunctionDeclaration output(
					original_symbol,
					operatoriness,
					ret_type,
					std::move(parameters),
					abi_wrapped.withoutValidation(),
					origin
				);
				abi_wrapped.withValidation(ctx, &output).valueOrThrow();

				this->out.emplace(std::move(output));
			}

			// @TODO: #1029 make failure more explicit
			void visitFun(pst::Access<pst::Fun> stmt) final {
				// @TODO: #1029 rest, flags, attributes, etc
				const auto operatoriness = operatorinessFromNameAndArity(
					name(original_symbol), stmt->getParams().unlock(ctx)->size()
				);
				emplaceDeclaration(stmt->getParams(), stmt->getRet(), operatoriness);
			}

			void visitFunDecl(pst::Access<pst::FunDecl> stmt) final {
				const auto operatoriness = operatorinessFromNameAndArity(
					name(original_symbol), stmt->getParams().unlock(ctx)->size()
				);
				emplaceDeclaration(stmt->getParams(), stmt->getRet(), operatoriness);
			}

			void visitMethod(pst::Access<pst::Method> stmt) final {
				// We can't use the interface directly because the interface
				// can use the declaration of function query, and we could get a cycle.
				auto specifiers = getClassMemberSpecifiers(ctx, original_symbol);
				if (specifiers.is_static) {
					const auto operatoriness = operatorinessFromNameAndArity(
						name(original_symbol), stmt->getParams().unlock(ctx)->size()
					);
					emplaceDeclaration(stmt->getParams(), stmt->getRet(), operatoriness);
					return;
				}
				const auto operatoriness = operatorinessFromNameAndArity(
					name(original_symbol), stmt->getParams().unlock(ctx)->size() + 1
				);
				// +1 for the implicit `self` parameter.
				emplaceDeclaration(stmt->getParams(), stmt->getRet(), operatoriness);

				const auto  self_scope  = ctx.query<QueryPrimaryCodeScopeFor>(stmt);
				const SymID self_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= defgen::SelfParameter{ .method_symbol = this->original_symbol,
				                             .scope         = self_scope },
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

			void visitCopyConstructor(pst::Access<pst::CopyConstructor> stmt) final {
				emplaceDeclaration(
					stmt->getParams(), {}, HOUTFunctionDeclaration::Operatoriness::None
				);

				const auto class_type  = classMemberOwner(original_symbol);
				this->out->return_type = tsh::SymbolType<>{
					class_type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			void visitDestructor(pst::Access<pst::Destructor> stmt) final {
				// A destructor takes an implicit `ref T` and returns unit
				const auto  self_scope  = ctx.query<QueryPrimaryCodeScopeFor>(stmt);
				const SymID self_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					.name = base::StrID("self"),
					.generated_symbol_data
					= defgen::SelfParameter{ .method_symbol = this->original_symbol,
				                             .scope         = self_scope },
				});

				std::vector<code::Parameter> parameters;
				parameters.emplace_back(
					name(self_symbol),
					ctx.query<QueryTypeOfSymbol>(self_symbol)->valueOrThrow(),
					std::nullopt,
					self_symbol,
					code::generatedOrigin()
				);

				const auto unit_ret = tsh::SymbolType<>::withDefaults(tsh::getUnitType());

				this->out.emplace(HOUTFunctionDeclaration(
					original_symbol,
					HOUTFunctionDeclaration::Operatoriness::None,
					unit_ret,
					std::move(parameters),
					DefaultAbi{},
					code::pstOrigin(stmt)
				));
			}
		};

		/**
		 * @brief Get the declaration of an implicit class constructor.
		 */
		static PResult getImplicitCtorDecl(Context& ctx, const defgen::Constructor& ctor_data) {
			// Preamble
			using defgen::Constructor;
			using defgen::Parameter;
			using std::ranges::to;
			using std::views::transform;

			// Get type data
			const auto target_type = ctor_data.type;
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
					   .name = ctor_name,
					   .generated_symbol_data
                = Constructor{ .type = target_type, .kind = Constructor::Kind::Implicit },
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
					.name                  = base::StrID(name(field.getSymbol())),
					.generated_symbol_data = Parameter{ .function_symbol = ctor_symbol,
				                                        .parameter_index = argument_index },
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

			// Check that this logic did not diverge from `getType()`.
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
				ctor_symbol,        HOUTFunctionDeclaration::Operatoriness::None,
				result_symbol_type, std::move(parameters),
				DefaultAbi{},       code::generatedOrigin(),
			};
		}

		/**
		 * @brief Get the declaration of a function-like symbol based on its type.
		 */
		static PResult funDeclFromType(
			Context& ctx, QKey fun, const HOUTFunctionDeclaration::Operatoriness operatoriness
		) {
			const auto builtin_type = ctx.query<QueryTypeOfSymbol>({ fun })
			                              ->valueOrThrow()
			                              .getType()
			                              .as<tsh::FunctionAbstractType>();
			const auto return_type = builtin_type.getResultType();
			auto       parameters  = std::vector<code::Parameter>{};
			for (u32 i = 0; const auto& param_type: builtin_type.getParameterTypes()) {
				const auto param_symbol = ctx.query<defgen::QueryGeneratedSymbol>({
					.name=base::StrID(base::strConcat("_", i).c_str()),
					.generated_symbol_data=defgen::Parameter{
						.function_symbol = fun,
						.parameter_index = i,
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
				fun,          operatoriness,           return_type, std::move(parameters),
				DefaultAbi{}, code::generatedOrigin(),
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			switch (kind(key)) {
			case SymbolKind::Function:
			case SymbolKind::FunctionDeclaration:
			case SymbolKind::Method:
			case SymbolKind::Constructor:
			case SymbolKind::Destructor: {
				variant_match(getSymRef(key)->other) {
					variant_case_novalue(
						PstImplementedSemantics, ClassMemberSemantics, BuiltinSemantics
					) {
						DeclarationVisitor decl_maker(ctx, key);
						stmt(ctx, key).value()->acceptVisitor(decl_maker);
						auto result = std::move(decl_maker.out).value();
						verifyFunctionAttributes(ctx, stmt(ctx, key).value(), result);
						return result;
					}
					variant_case(defgen::Constructor, ctor_data) {
						switch (ctor_data.kind) {
						case defgen::Constructor::Kind::Implicit:
							return getImplicitCtorDecl(ctx, ctor_data);
						case defgen::Constructor::Kind::Default: {
							const auto return_type = tsh::SymbolType<>{ ctor_data.type,
								                                        tsh::ReferenceKind::Direct,
								                                        tsh::Mutability::Mutable };

							return HOUTFunctionDeclaration{
								key,          HOUTFunctionDeclaration::Operatoriness::None,
								return_type,  {},
								DefaultAbi{}, code::generatedOrigin(),
							};
						}
						case defgen::Constructor::Kind::Copy:
							return funDeclFromType(
								ctx, key, HOUTFunctionDeclaration::Operatoriness::None
							);
						}
						CORE_UNREACHABLE();
					}
					variant_case_novalue(
						defgen::Method,
						defgen::BuiltinTemplatedSymbol,
						defgen::ReplInputWrapper,
						defgen::ScriptMainWrapper
					) {
						return funDeclFromType(
							ctx, key, HOUTFunctionDeclaration::Operatoriness::None
						);
					}
					variant_case(defgen::BuiltinOperator, builtin_op) {
						// All builtins are unary or binary operators.
						return funDeclFromType(ctx, key, builtin_op.operatoriness);
					}
					variant_default {
						// Other symbols are not functions.
						CORE_UNREACHABLE();
					}
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
