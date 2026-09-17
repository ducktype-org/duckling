#include "query_type_of_symbol.hpp"

#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios/hout/elements/stmt.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/deductions.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/main_return_type.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/definition_generation/default_destructors.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <query_framework/standard_query/query_impl.hpp>

#include <utility>

namespace compiler::helios {


	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, QuerySymbolType_Result) {
		class PstVisitor_GetTypeOf final: public pst::PstVisitorPanicky {
			Context& ctx;

			void setFailed() { symbol_type_qresult = query::Failed(); }

			void setTypeOfSymbol(const tsh::SymbolType<>& type) {
				if (symbol_type_qresult.hasValue())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type_qresult = type;
			}

			void setTypeOfSymbolByAbstractType(const tsh::AbstractType& type) {
				if (symbol_type_qresult.hasValue())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type_qresult = tsh::SymbolType<>::withDefaults(type);
			}

			void setSymbolTypeByTypeExpr(const pst::Access<pst::ExprElement> expr) {
				const auto type_ctv = getTypeCTVFromPST(ctx, expr);
				if (type_ctv.hasFailed()) {
					setFailed();
					return;
				}
				setTypeOfSymbol(type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value());
			}

			void setSymbolTypeByTypeExpr(
				const pst::Access<pst::ExprElement> expr, const tsh::Mutability expected_mutability
			) {
				const auto type_ctv = getTypeCTVFromPST(ctx, expr);
				if (type_ctv.hasFailed()) {
					setFailed();
				} else {
					setTypeOfSymbol(tsh::deductions::declarationTypeFromProvidedType(
						type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value(), expected_mutability
					));
				}
			}

		public:
			PstVisitor_GetTypeOf(Context& ctx): ctx(ctx) {}

			query::QResult<tsh::SymbolType<>> symbol_type_qresult = query::Failed();

			void visitConst(pst::Access<pst::Const> stmt) final {
				if (stmt->getType().has_value()) {
					setSymbolTypeByTypeExpr(
						stmt->getType().value().unlock(ctx)->getExpr().unlock(ctx),
						tsh::Mutability::Immutable
					);
				} else if (stmt->getValue().has_value()) {
					auto parsed = ctx.query<QueryHoutOfExpr>(
						{ stmt->getValue().value().unlock(ctx)->getExpr() }
					);
					if (parsed->hasFailed()) {
						setFailed();
						return;
					}
					const auto& expr_type = parsed->valueOrThrow()->expression_type;
					setTypeOfSymbol(tsh::deductions::declarationTypeFromInitializer(
						expr_type, tsh::Mutability::Immutable
					));
				} else {
					CORE_PANIC(
						"Variable declaration without type or value, this should not parse in the "
						"parser."
					);
				}
			}

			void visitVariable(pst::Access<pst::Variable> stmt) final {
				const auto& decl_mutability
					= stmt->isConst() ? tsh::Mutability::Immutable : tsh::Mutability::Mutable;
				if (stmt->getType().has_value()) {
					setSymbolTypeByTypeExpr(
						stmt->getType().value().unlock(ctx)->getExpr().unlock(ctx), decl_mutability
					);
				} else if (stmt->getValue().has_value()) {
					auto& parsed = ctx.query<QueryHoutOfExpr>(
										  { stmt->getValue().value().unlock(ctx)->getExpr() }
					)
					                   ->valueOrThrow();

					const auto& expr_type = parsed->expression_type;
					setTypeOfSymbol(
						tsh::deductions::declarationTypeFromInitializer(expr_type, decl_mutability)
					);
				} else {
					CORE_PANIC(
						"Variable declaration without type or value, this should not parse in the "
						"parser."
					);
				}
			}

			void visitField(pst::Access<pst::Field> field) final {
				const auto decl_mutability
					= field->isMutable() ? tsh::Mutability::Mutable : tsh::Mutability::Immutable;
				setSymbolTypeByTypeExpr(
					field->getType().unlock(ctx)->getExpr().unlock(ctx), decl_mutability
				);
			}

			void visitClass(pst::Access<pst::Class>) final {
				setTypeOfSymbolByAbstractType(tsh::getMetaType());
			}

			void visitNamespace(pst::Access<pst::Namespace>) final {
				setTypeOfSymbolByAbstractType(tsh::getNamespaceType());
			}

			void visitImport(pst::Access<pst::Import>) final {
				setTypeOfSymbolByAbstractType(tsh::getImportType());
			}

			void visitParam(pst::Access<pst::Param> param) final {
				// @TODO: #1396 Handle parameter mutability.
				setSymbolTypeByTypeExpr(
					param->getType().unlock(ctx)->getExpr().unlock(ctx), tsh::Mutability::Mutable
				);
			}

			void visitTemplateStmt(pst::Access<pst::TemplateStmt> stmt) final {
				// @TODO: #3177 this now always uses QueryTypeTemplateType,
				// we should probably introduce different kind of types for non-type templtes.

				auto symbol  = ctx.query<QuerySymbolOfSTMT>({ stmt }).valueOrThrow();
				auto ab_type = ctx.query<tsh::QueryTypeTemplateType>({ .source = symbol });
				setTypeOfSymbolByAbstractType(ab_type);
			}

			void visitIdentifierWrapper(pst::Access<pst::IdentifierWrapper> ident) final {
				// @TODO: #2782 Remove this function.
				auto parent_opt = ident->getParent();
				CORE_ASSERT(parent_opt.has_value(), "IdentifierWrapper without in type query");

				auto parent_elem = parent_opt.value().unlock(ctx);
				if (auto for_stmt_opt = parent_elem.dynamicCast<pst::For>()) {
					handleForIterator(for_stmt_opt.value());
					return;
				}
				if (auto binding_opt = parent_elem.dynamicCast<pst::BindingPattern>()) {
					handleMatchBinding(binding_opt.value());
					return;
				}
				CORE_PANIC("IdentifierWrapper with unsupported parent in QueryTypeOfSymbol");
			}

			void handleMatchBinding(pst::Access<pst::BindingPattern> binding) {
				// The binding's type is the type constraint of the enclosing flow pattern
				// (`case x : T`). Bindings without a constraint are not supported yet.
				auto flow_parent = binding->getParent();
				CORE_ASSERT(flow_parent.has_value(), "BindingPattern without parent");
				auto flow_opt = flow_parent.value().unlock(ctx).dynamicCast<pst::FlowPattern>();
				if (!flow_opt.has_value()) {
					ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
						"Pattern bindings outside of a flow pattern.", binding->getStablePosition()
					));
					setFailed();
					return;
				}

				auto constraint = flow_opt.value()->getTypeConstraint();
				if (!constraint.has_value()) {
					ctx.logInt(makeBox<dia::NotYetImplementedCodeError>(
						"Match pattern bindings without a type constraint (`case x : T`).",
						binding->getStablePosition()
					));
					setFailed();
					return;
				}

				setSymbolTypeByTypeExpr(constraint.value().unlock(ctx)->getExpr().unlock(ctx));
			}

			void handleForIterator(pst::Access<pst::For> stmt) {
				// Get the element type from the iterable.
				auto iterable_pst  = stmt->getIterable().unlock(ctx)->getExpr();
				auto iterable_hout = ctx.query<QueryHoutOfExpr>({ iterable_pst });
				if (iterable_hout->hasFailed()) {
					setFailed();
					return;
				}
				tsh::SymbolType<> iterable_type
					= iterable_hout->valueOrThrow()->expression_type.getSymbolType();

				auto iterable_kind = iterable_type.getType().getKind();
				auto element_type  = [&]() -> base::Optional<tsh::SymbolType<>> {
                    switch (iterable_kind) {
                    case tsh::Kind::StaticArray:
                        return iterable_type.getType()
                            .as<tsh::StaticArrayAbstractType>()
                            .getElementType();
                    default:
                        return {};
                    }
				}();
				if (!element_type.has_value()) {
					// We don't log a NYI error here, since it's logged in the
					// `desugaring::desugarFor()`.
					setFailed();
					return;
				}

				// First assume that the iterator is the same as the element type of the array.
				tsh::SymbolType<> iter_type = element_type.value();

				// Now look for the explicit type annotation which will potentially override the
				// deduced type.
				if (auto type_holder_opt = stmt->getIteratorType().unlockOpt(ctx)) {
					if (auto maybe_iter_type_pst
					    = type_holder_opt.value()->getExpr().unlockOpt(ctx)) {
						// If a type exists we use it.
						auto type_ctv = getTypeCTVFromPST(ctx, maybe_iter_type_pst.value());
						if (type_ctv.hasFailed()) {
							setFailed();
							return;
						}
						iter_type = type_ctv.valueOrThrow().get<tsh::SymbolType<>>().value();
					}
				}

				if (auto maybe_is_const = stmt->getIsConst(); maybe_is_const.has_value()) {
					iter_type = iter_type.withMutability(
						maybe_is_const.value() ? tsh::Mutability::Immutable
											   : tsh::Mutability::Mutable
					);
				} else {
					// If no let/var exists the element type is the same as the array element type.
					// If the array stores a const than the iterator is const.
				}
				setTypeOfSymbol(iter_type);
			}
		};

		static auto handleFunction(Context& ctx, SymID sym) {
			// @note: this crates false dependency of default parameter expressions
			auto& declaration = ctx.query<QueryDeclOfFun>(sym)->valueOrThrow();
			std::vector<tsh::SymbolType<>> param_types{};
			param_types.reserve(declaration.parameters.size());
			for (auto& param: declaration.parameters) param_types.emplace_back(param.type);

			return tsh::SymbolType{
				ctx.query<tsh::QueryFunctionType>({ .parameter_types = param_types,
				                                    .result_type     = declaration.return_type }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			variant_match(symbol_ref->other) {
				variant_case_novalue(
					PstImplementedSemantics, ClassMemberSemantics, BuiltinSemantics
				) {
					// @note: function are handled in a special way, using QueryDeclOfFun.
					if (isFunctionLike(kind(key))) return handleFunction(ctx, key);

					PstVisitor_GetTypeOf visitor(ctx);
					maybeSymbolPst(key)->unlock(ctx)->acceptVisitor(visitor);

					return visitor.symbol_type_qresult;
				}
				variant_case(defgen::Constructor, ctor) {
					const auto target_type = ctor.type;

					// @TODO: #1328 Properly handle value categories in class constructors.
					// The implicit constructor takes a parameter per field, the default constructor
					// takes none, and the copy constructor takes a single `const ref T` source.
					std::vector<tsh::SymbolType<>> param_types;
					switch (ctor.kind) {
					case defgen::Constructor::Kind::Implicit: {
						auto fields = target_type.getInterface(ctx)->getFieldsView();
						for (const auto& field: fields) param_types.push_back(field.getType(ctx));
						break;
					}
					case defgen::Constructor::Kind::Default:
						break;
					case defgen::Constructor::Kind::Copy:
						param_types.emplace_back(
							target_type, tsh::ReferenceKind::Ref, tsh::Mutability::Immutable
						);
						break;
					}

					const auto return_type = tsh::SymbolType<>::withDefaults(target_type);

					const auto ctor_abstract_type = ctx.query<tsh::QueryFunctionType>({
						.parameter_types = std::move(param_types),
						.result_type     = return_type,
					});

					return tsh::SymbolType<>{
						ctor_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(defgen::Method, method) {
					// Mutable self type.
					const auto mut_self = tsh::SymbolType<>{ method.owner_type,
						                                     method.owner_type.isSimple()
						                                         ? tsh::ReferenceKind::Direct
						                                         : tsh::ReferenceKind::Ref,
						                                     tsh::Mutability::Mutable };
					// Immutable self type
					const auto immmut_self = mut_self.withMutability(tsh::Mutability::Immutable);


					// The self parameter and return type depend on the concrete method kind.
					auto [arg_types, return_type]
						= [&]() -> std::pair<std::vector<tsh::SymbolType<>>, tsh::SymbolType<>> {
						switch (method.kind) {
						case defgen::Method::Kind::ToString:
							return { { immmut_self },
								     tsh::SymbolType<>::withDefaults(tsh::getStringType(ctx)) };
						case defgen::Method::Kind::LengthMethod:
							return { { immmut_self },
								     tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
										 ctx, 64, tsh::IntegralAbstractType::Signedness::Signed
									 )) };
						case defgen::Method::Kind::DefaultDestructor:
							return { { mut_self },
								     tsh::SymbolType<>{ tsh::getUnitType(),
								                        tsh::ReferenceKind::Direct,
								                        tsh::Mutability::Immutable } };
						}
						CORE_UNREACHABLE();
					}();

					const auto method_abstract_type = ctx.query<tsh::QueryFunctionType>(
						{ .parameter_types = std::move(arg_types), .result_type = return_type }
					);

					return tsh::SymbolType<>{
						method_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(defgen::BuiltinOperator, op) {
					return tsh::SymbolType<>{
						op.operator_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case(defgen::BuiltinTemplatedSymbol, builtin) {
					auto [arg_types, return_type]
						= [&]() -> std::pair<std::vector<tsh::SymbolType<>>, tsh::SymbolType<>> {
						switch (builtin.kind) {
						case defgen::BuiltinTemplatedSymbol::Kind::MoveIn: {
							const auto ptr_type = tsh::SymbolType<>::withDefaults(
								ctx.query<tsh::QueryPointerType>({ builtin.type })
							);
							return {
								{ ptr_type, builtin.type },
								tsh::SymbolType<>::withDefaults(tsh::getUnitType()),
							};
						}
						}

						CORE_UNREACHABLE();
					}();

					const auto fn_type = ctx.query<tsh::QueryFunctionType>(
						{ .parameter_types = std::move(arg_types), .result_type = return_type }
					);
					return tsh::SymbolType<>::withDefaults(fn_type);
				}
				variant_case(defgen::GeneratedConstant, gen_const) {
					return gen_const.value.getTypeOfStoredValue(ctx);
				}
				variant_case(defgen::Parameter, param) {
					const auto function_type
						= ctx.query<QueryTypeOfSymbol>({ param.function_symbol })
					          ->valueOrThrow()
					          .getType()
					          .as<tsh::FunctionAbstractType>();
					auto param_symbol_type
						= function_type.getParameterTypes().at(param.parameter_index);
					return param_symbol_type;
				}
				variant_case(defgen::SelfParameter, param) {
					const auto class_type        = classMemberOwner(param.method_symbol);
					auto       param_symbol_type = tsh::SymbolType{
                        class_type,
                        tsh::ReferenceKind::Ref,
                        tsh::Mutability::Mutable,
					};
					return param_symbol_type;
				}
				variant_case(defgen::Field, field) {
					// @TODO: #2515 Implement other cases
					switch (field.parent_type.getKind()) {
					case tsh::Kind::Tuple:
						return field.parent_type.as<tsh::TupleAbstractType>().getComponents().at(
							field.index
						);
					case tsh::Kind::Slice: {
						if (field.index == 0) {
							auto element_type
								= field.parent_type.as<tsh::SliceAbstractType>().getElementType();
							auto many_pointer_type
								= ctx.query<tsh::QueryManyPointerType>({ element_type });
							return tsh::SymbolType<>::withDefaults(many_pointer_type);
						}
						if (field.index == 1) {
							// The length is signed, so it mixes with the (signed) index type
							// without a cast.
							return tsh::SymbolType<>::withDefaults(tsh::getIntegralType(
								ctx, 64, tsh::IntegralAbstractType::Signedness::Signed
							));
						}
						CORE_PANIC("Slice only has fields 0 (element) and 1 (length)");
					}
					default:
						CORE_UNREACHABLE();
					}
				}
				variant_case(defgen::GeneratedFunctionVariable, var) { return var.type; }
				variant_case(defgen::ControlFlowLocal, local) { return local.type; }
				variant_case(defgen::ReplEmptyVariable, empty_variable) {
					return ctx.query<QueryTypeOfSymbol>(empty_variable.original_variable)
					    ->valueOrThrow();
				}
				variant_case(defgen::ReplInputWrapper, repl) {
					const auto function_abstract_type = ctx.query<tsh::QueryFunctionType>({
						.parameter_types = {},
						.result_type     = repl.return_type,
					});
					return tsh::SymbolType<>{
						function_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_case_novalue(defgen::ScriptMainWrapper) {
					const auto return_type = requiredMainReturnType(ctx);
					const auto function_abstract_type
						= ctx.query<tsh::QueryFunctionType>({ .parameter_types = {},
					                                          .result_type     = return_type });
					return tsh::SymbolType<>{
						function_abstract_type,
						tsh::ReferenceKind::Direct,
						tsh::Mutability::Immutable,
					};
				}
				variant_default { CORE_PANIC("Unknown symbol data type"); }
			}
			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);

	base::Optional<SymID> getTypeDestructor(query::Context& ctx, tsh::SymbolType<> symbol_type) {
		return defgen::destructSymForSymbolType(ctx, symbol_type);
	}
}
