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
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/hout_creation/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <query_framework/standard_query/query_impl.hpp>

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
				symbol_type_qresult = tsh::SymbolType{
					type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
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
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Pattern bindings outside of a flow pattern.", binding->getStablePosition()
					));
					setFailed();
					return;
				}

				auto constraint = flow_opt.value()->getTypeConstraint();
				if (!constraint.has_value()) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Match pattern bindings without a type constraint (`case x : T`).",
						binding->getStablePosition()
					));
					setFailed();
					return;
				}

				setSymbolTypeByTypeExpr(
					constraint.value().unlock(ctx)->getExpr().unlock(ctx), tsh::Mutability::Immutable
				);
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
                    case tsh::Kind::DynamicArray:
                        return iterable_type.getType()
                            .as<tsh::DynamicArrayAbstractType>()
                            .getElementType();
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
				ctx.query<tsh::QueryFunctionType>({ param_types, declaration.return_type }),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			variant_match(symbol_ref->other) {
				variant_case(PstSymbolData, pst_data) {
					// @note: function are handled in a special way, using QueryDeclOfFun.
					if (kind(key) == SymbolKind::Function
					    or kind(key) == SymbolKind::FunctionDeclaration
					    or kind(key) == SymbolKind::Method)
						return handleFunction(ctx, key);

					PstVisitor_GetTypeOf visitor(ctx);
					pst_data.getElement().unlock(ctx)->acceptVisitor(visitor);

					return visitor.symbol_type_qresult;
				}
				variant_case(defgen::GeneratedSymbolData, generated_data) {
					return generated_data.getType(ctx);
				}
				variant_default { CORE_PANIC("Unknown symbol data type"); }
			}
			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);
}
