#pragma once

#include <typesystem/higher/type_info.hpp>
#include <typesystem/lower/type_layout.hpp>

#include <query_framework/query_int.hpp>

namespace tsl {
	DECLARE_QUERY(QueryTypeLayout, tsh::TypeInfo, TypeLayout)
}
