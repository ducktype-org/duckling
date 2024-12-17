#pragma once

#include <query_framework/query_int.hpp>
#include <pst_parser/elements/elements_list.hpp>
#include <typesystem/higher/type_info.hpp>

#include <helios/helios_errors.hpp>
#include "../helios_result.hpp"

namespace compiler::helios {

	using TypeEvalResult = errors::HResult<tsh::TypeInfo, errors::Failed>;

	struct KeyOf_EvalExprToType {
		MCRef<pst::ExprElement> expr;


		bool operator==(const KeyOf_EvalExprToType&) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const;
	};

	/**
	 * Given the PST expression, parses it and evaluates this expression to type
	 * @return tsh::TypeInfo with information about the evaluated type.
	 * @todo should this return type info or type desc, we should have a document
	 * defining which one is which
	 */
	DECLARE_QUERY(EvalExprToType, KeyOf_EvalExprToType, TypeEvalResult)
}
