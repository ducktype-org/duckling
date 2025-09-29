#pragma once

#include <query_framework/query_int.hpp>
#include <helios/hout/hout.hpp>
#include <typesystem/higher/types.hpp>

namespace compiler::helios::houtgen {
	DECLARE_QUERY(QueryImplicitClassConstructor, tsh::ClassAbstractType, CRef<HOUTFunction>);
}
