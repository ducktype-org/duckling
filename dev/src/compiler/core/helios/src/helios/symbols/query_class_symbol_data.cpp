
#include "query_class_symbol_data.hpp"

#include "symbol_id_utils.hpp"
#include "symbol_kind.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/class.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/class_block.hpp>
#include <frontend/pst_parser/elements/includes/basic.hpp>
#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios_private/comp_time/comp_time.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryClassSymbolData, QueryClassSymbolData_Result) {
		struct ClassDataParser final: pst::PstVisitorPanicky {
			query::Context& ctx;

			ClassDataParser(query::Context& ctx): ctx(ctx) {}

			base::Optional<base::StrID>                            name;
			base::Optional<pst::AccessLocked<pst::ExprElement>>    base_class;
			base::Optional<pst::AccessLocked<pst::ImplementsList>> implements;

			void visitClass(pst::Access<pst::Class> stmt) final {
				name = stmt->getName().unlock(ctx)->unwrap();
				if (auto base = stmt->getBase().unlockOpt(ctx))
					base_class = base.value()->getExpr().unlock(ctx);
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
				UNPACK_QRESULT(auto ctv =, getTypeCTVFromPST(ctx, base));
				// @TODO: #1630 Raise errors, here, or preferably earlier, if the symbol
				// type of the base class has any specifiers.
				class_info.base = ctv.get<tsh::SymbolType<>>()->getType();
				class_info.implements.push_back(ctv.get<tsh::SymbolType<>>()->getType());
			}

			if_opt_some(class_data_parser.implements, implements) {
				for (auto&& interface: *implements.unlock(ctx)) {
					UNPACK_QRESULT(
						auto ctv =, getTypeCTVFromPST(ctx, interface.unlock(ctx)->getExpr())
					);
					// @TODO: #1630 Raise errors, here, or preferably earlier, if the symbol
					// type of the base class has any specifiers.
					class_info.implements.push_back(ctv.get<tsh::SymbolType<>>()->getType());
				}
			}

			return class_info;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryClassSymbolData);
}
