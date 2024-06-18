#include "type_interface.hpp"

#include "typesystem/queries/types.hpp"

#include <query_framework/query_impl.hpp>

namespace ts {
	TypeInfo InterfaceElement::getType(query::Context& ctx) const {
		if (isField())
			return getResultType();
		else {
			// @TODO: Add .is_mutable and .pure when additional method specifiers are supported.
			std::vector<TypeInfo> all_parameter_types{};
			all_parameter_types.push_back(ctx.query<QueryPointerType>({
				.type       = source,
				.is_mutable = true,
			}));
			for (const auto& p_type: parameter_types.value()) all_parameter_types.push_back(p_type);
			return ctx.query<QueryFunctionType>({
				.parameter_types = all_parameter_types,
				.result_type     = result_type,
				.pure            = false,
				.free            = false,
			});
		}
	}
}
