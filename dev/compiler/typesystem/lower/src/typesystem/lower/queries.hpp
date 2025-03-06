#pragma once

#include <typesystem/higher/abstract_type.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <query_framework/query_int.hpp>

namespace tsl {
	DECLARE_QUERY(QueryTypeLayout, tsh::AbstractType, TypeLayout)
}
