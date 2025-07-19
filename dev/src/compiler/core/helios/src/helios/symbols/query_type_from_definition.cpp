
#include "query_type_from_definition.hpp"

#include <helios_private/symbols/symbol_data.hpp>
#include <pst_parser/pst_visitor.hpp>
#include <typesystem/higher/queries/types.hpp>

#include <query_framework/query_impl.hpp>

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
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto symbol_ref = getSymRef(key);

			PstVisitor_GetTypeFromDefinition visitor(ctx, key);
			symbol_ref->getPSTData()->pst_element.unlock(ctx)->acceptVisitor(visitor);
			return visitor.definition_symbol_type.value();
		}

		QUERY_AUTO_CACHE_REF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTypeFromDefinition)
}
