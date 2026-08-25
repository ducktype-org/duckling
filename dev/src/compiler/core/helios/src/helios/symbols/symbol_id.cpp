#include "symbol_id.hpp"

#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>
#include <helios_private/symbols/symbol_data.hpp>

namespace compiler::helios {

	u64 SymID::queryUnstablePerfectHash() const { return ref->id.asInt(); }

	tsh::ClassAbstractType classOfMember(query::Context& ctx, const SymID member) {
		CORE_ASSERT(isClassMember(kind(member)), "Expected a member of a class symbol.");

		auto member_data = getSymRef(member)->getDataOpt<ClassMemberSemantics>();
		CORE_ASSERT(member_data.has_value(), "Expected a class member defined in the PST.");

		return ctx.query<tsh::QueryClassType>(member_data.value()->owner_class);
	}
}
