#pragma once

#include <helios/hout/hout.hpp>
#include <typesystem/higher/types.hpp>

#include <query_framework/query_int.hpp>

namespace compiler::helios::houtgen {
	DECLARE_QUERY(QueryImplicitClassConstructor, tsh::ClassAbstractType, CRef<HOUTFunction>);
}
