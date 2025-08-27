
#include "query_class_symbol_data.hpp"

#include "simple.hpp"
#include "symbol_kind.hpp"

#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <pst_parser/elements/hierarchy/declarations/class.hpp>
#include <pst_parser/elements/hierarchy/not_statements/class_block.hpp>
#include <pst_parser/elements/includes/basic.hpp>
#include <pst_parser/pst_visitor.hpp>

#include <query_framework/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstVisitorPanicky {
			query::Context& ctx;

			ClassDataParser(query::Context& ctx): ctx(ctx) {}

			base::Optional<base::StrID>                            name;
			base::Optional<pst::AccessLocked<pst::ExprElement>>    base_class;
			base::Optional<pst::AccessLocked<pst::ImplementsList>> implements;

			void visitClass(pst::Access<pst::Class> stmt) final {
				name = stmt->getName();
				if (auto base = stmt->getBase().unlockOpt(ctx)) base_class = base.value();
				if (auto implements = stmt->getImplements().unlockOpt(ctx))
					this->implements = implements.value();
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(kind(key) == SymbolKind::Class, "Symbol is not a class");

			auto class_stmt = getSymRef(key)->stmtCast(ctx).value();

			auto class_body_scope = queryBodyCodeScopeFor(ctx, class_stmt);
			auto class_symbols    = ctx.query<QuerySymbolsInScope>(class_body_scope);

			ClassSymbolData class_info;
			for (auto sym: *class_symbols) {
				switch (kind(sym)) {
				case SymbolKind::Method:
					class_info.methods.push_back(sym);
					break;
				case SymbolKind::Constructor:
					class_info.constructors.push_back(sym);
					break;
				case SymbolKind::Destructor:
					// This doesn't catch multiple destructors
					class_info.destructor = sym;
					break;
				case SymbolKind::Field:
					class_info.members.push_back(sym);
					break;
				default:
					throw base::NotYetImplemented(base::strConcat(
						"Using ", typeid(kind(sym)).name(), " inside a class is not yet implemented."
					));
				}
			}
			// Find the name
			auto class_data_parser = ClassDataParser(ctx);
			class_stmt->acceptVisitor(class_data_parser);
			class_info.name = class_data_parser.name.value();

			if_opt_some(class_data_parser.base_class, base) {
				if (auto ctv = ctx.query<QueryEvaluateExpression>({ base })) {
					if (auto maybe_type = ctv.value().asType()) {
						// @TODO: Raise errors, here, or preferably earlier, if the symbol type of
						// the base class is given with any specifiers apart from the abstract type.
						class_info.base = maybe_type.value().getType();
					} else {
						return query::QError(errors::Failed());
					}
				} else {
					// We just fail here, error should be reported by QueryEvaluateExpression.
					return query::QError(errors::Failed());
				}
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements.unlock(ctx)) {
					if (auto ctv
					    = ctx.query<QueryEvaluateExpression>(interface.unlock(ctx)->getExpr())) {
						if (auto maybe_type = ctv.value().asType()) {
							// @TODO: Raise errors, here, or preferably earlier, if the symbol type
							// of the base class is given with any specifiers apart from the
							// abstract type.
							class_info.implements.push_back(maybe_type.value().getType());
						} else {
							return query::QError(errors::Failed());
						}
					} else {
						// We just fail here, error should be reported by QueryEvaluateExpression.
						return query::QError(errors::Failed());
					}
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);

}
