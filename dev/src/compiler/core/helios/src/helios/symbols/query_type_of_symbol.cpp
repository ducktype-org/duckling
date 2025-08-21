#include "query_type_of_symbol.hpp"

#include <helios_private/comp_time/type_eval.hpp>
#include <helios_private/expressions/query_hout_of_expr.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <pst_parser/elements/hierarchy/lists/all_lists.hpp>
#include <pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <typesystem/higher/expression_type.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios {


	struct IMPLEMENT_QUERY(QueryTypeOfSymbol, QuerySymbolType_Result) {
		class PstVisitor_GetTypeOf final: public pst::PstVisitorPanicky {
			Context& ctx;

			// @TODO: make failure more explicit

			void setTypeOfSymbol(const tsh::SymbolType<>& type) {
				if (symbol_type.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type = type;
			}

			void setTypeOfSymbolByAbstractType(const tsh::AbstractType& type) {
				if (symbol_type.has_value())
					CORE_PANIC("Attempted to set type of symbol in visitor a second time.");
				symbol_type = tsh::SymbolType{
					type,
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			void setTypeOfSymbol(pst::Access<pst::ExprElement> expr) {
				auto tp = ctx.query<EvalExprToType>(pst::AccessLocked<pst::ExprElement>(expr));
				if (tp.hasValue()) setTypeOfSymbol(tp.value());
			}

		public:
			PstVisitor_GetTypeOf(Context& ctx): ctx(ctx) {}

			base::Optional<tsh::SymbolType<>> symbol_type;

			void visitConst(pst::Access<pst::Const> stmt) final {
				if (stmt->getType().has_value()) {
					setTypeOfSymbol(stmt->getType().value().unlock(ctx)->getExpr().unlock(ctx));
				} else if (stmt->getValue().has_value()) {
					auto parsed = (ctx.query<QueryHoutOfExpr>(
						{ stmt->getValue().value().unlock(ctx)->getExpr() }
					));
					if (parsed.hasValue()) {
						const auto& expr_type = parsed.value()->expression_type;
						setTypeOfSymbol(tsh::deduceTypeFromExpressionType(expr_type));
					} else
						throw base::NotYetImplemented(
							"Const declaration with value that does not evaluate to a type. This "
							"should be a compilation error"
						);
				} else {
					CORE_PANIC(
						"Variable declaration without type or value, this should not parse in the "
						"parser."
					);
				}
			}

			void visitVariable(pst::Access<pst::Variable> stmt) final {
				if (stmt->getType().has_value()) {
					setTypeOfSymbol(stmt->getType().value().unlock(ctx)->getExpr().unlock(ctx));
				} else if (stmt->getValue().has_value()) {
					auto parsed = (ctx.query<QueryHoutOfExpr>(
						{ stmt->getValue().value().unlock(ctx)->getExpr() }
					));
					if (parsed.hasValue()) {
						const auto& expr_type = parsed.value()->expression_type;
						setTypeOfSymbol(tsh::deduceTypeFromExpressionType(expr_type));
					} else
						throw base::NotYetImplemented(
							"Const declaration with value that does not evaluate to a type. This "
							"should be a compilation error"
						);
				} else {
					CORE_PANIC(
						"Variable declaration without type or value, this should not parse in the "
						"parser."
					);
				}
			}

			void visitField(pst::Access<pst::Field> field) final {
				setTypeOfSymbol(field->getType().unlock(ctx)->getExpr().unlock(ctx));
			}

			void visitFun(pst::Access<pst::Fun> fun) final {
				auto locked_params = fun->getParams();
				auto params        = locked_params.unlock(ctx);
				auto ret           = fun->getRet();

				std::vector<tsh::SymbolType<>> param_types{};
				param_types.reserve(params->size());

				for (auto param: *params) {
					auto param_symbol = ctx.query<QuerySymbolOfSTMT>({ param });
					auto param_type   = ctx.query<QueryTypeOfSymbol>({ param_symbol });

					if (param_type->hasValue()) {
						param_types.emplace_back(param_type->value());
					} else {
						// we just fail here, because we can't continue without type
						return;
					}
				}

				// Default return type is a direct unit.
				tsh::SymbolType<> ret_type = tsh::SymbolType<>{
					ctx.query<tsh::QueryUnitType>({}),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};

				if (ret.has_value()) {
					auto parsed = ctx.query<EvalExprToType>(ret.value().unlock(ctx)->getExpr());
					if (parsed.hasValue()) {
						ret_type = parsed.value();
					} else {
						// we just fail here, because we can't continue without type
						return;
					}
				}
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryFunctionType>({ param_types,
				                                                                  ret_type }));
			}

			void visitClass(pst::Access<pst::Class>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryMetaType>({}));
			}

			void visitNamespace(pst::Access<pst::Namespace>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryNamespaceType>({}));
			}

			void visitImport(pst::Access<pst::Import>) final {
				setTypeOfSymbolByAbstractType(ctx.query<tsh::QueryImportType>({}));
			}

			void visitParam(pst::Access<pst::Param> param) final {
				setTypeOfSymbol(param->getType().unlock(ctx)->getExpr().unlock(ctx));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			variant_match(symbol_ref->other) {
				variant_case(PstSymbolData, pst_data) {
					PstVisitor_GetTypeOf visitor(ctx);
					pst_data.pst_element.unlock(ctx)->acceptVisitor(visitor);
					if_opt_some(visitor.symbol_type, type) { return type; }
					return query::QError(errors::Failed());
				}
				variant_case(builtin::BuiltinFunctionData, builtin_data) {
					return tsh::SymbolType<>(
						builtin_data.type, tsh::ReferenceKind::Direct, tsh::Mutability::Mutable
					);
				}
				variant_default { CORE_PANIC("Unknown symbol data type"); }
			}
			CORE_UNREACHABLE();
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeOfSymbol);
}
