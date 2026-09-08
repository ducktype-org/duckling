#pragma once

#include <helios/tsh/symbol_type.hpp>

#include <query_framework/context/context_fd.hpp>

namespace compiler::helios {
	/**
	 * @return the main return type required by the toolchain entry points.
	 */
	tsh::SymbolType<> requiredMainReturnType(query::Context& ctx);
}
