
#include "query_type_from_definition.hpp"

#include <frontend/pst_parser/pst_visitor.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <helios/tsh/queries/types.hpp>

#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryTypeFromDefinition, QueryTypeFromDefinition_Result) {
		class PstVisitor_GetTypeFromDefinition final: public pst::PstVisitorPanicky {
			Context&    ctx;
			const QKey& key;

			void setTypeOfDefinition(const tsh::SymbolType<>& type) {
				if (definition_symbol_type.has_value())
					CORE_PANIC("Attempted to set type of definition in visitor a second time.");
				definition_symbol_type = type;
			}

		public:
			PstVisitor_GetTypeFromDefinition(Context& ctx, const QKey& key): ctx(ctx), key(key) {}

			base::Optional<tsh::SymbolType<>> definition_symbol_type;

			void visitClass(pst::Access<pst::Class>) final {
				definition_symbol_type = tsh::SymbolType<>{
					ctx.query<tsh::QueryClassType>(key),
					tsh::ReferenceKind::Direct,
					tsh::Mutability::Mutable,
				};
			}

			void visitConst(pst::Access<pst::Const>) final {
				auto ctv_result = ctx.query<QueryConstValueOf>(key);
				if (ctv_result.hasFailed()) return;
				const auto& ctv           = ctv_result.valueOrThrow();
				const auto& type_of_const = ctv.get<tsh::SymbolType<>>();
				definition_symbol_type    = type_of_const;
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			PstVisitor_GetTypeFromDefinition visitor(ctx, key);
			symbol_ref->getPSTData()->getElement().unlock(ctx)->acceptVisitor(visitor);
			return visitor.definition_symbol_type.value();
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeFromDefinition)
}
