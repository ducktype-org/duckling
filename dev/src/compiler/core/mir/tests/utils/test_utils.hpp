#pragma once

#include <frontend/module_tree/module_id.hpp>
#include <helios/queries/queries.hpp>
#include <mir/mir_lowering/mir_queries.hpp>
#include <mir/mir_structure/mir_structure.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>

namespace compiler::mir::test_utils {
	inline CRef<mir::Function> getMIRFunctionByName(
		frontend::ModuleID module_id, std::string_view name
	) {
		auto result = query::utils::withContextCompute([&](query::Context& ctx) {
			auto& unit
				= ctx.query<compiler::helios::QueryTopLevelEntities>(module_id)->valueOrPanic();
			for (const auto& function: unit.functions) {
				if (function->declaration->original_name.strView() == name) {
					CRef<mir::Function> foo_mir
						= &ctx.query<compiler::mir::LowerToMIRFunction>({ function })->valueOrPanic();
					return foo_mir;
				}
			}
			CORE_PANIC(base::strConcat("Function with name '", name, "' not found in module "));
		});
		return std::any_cast<CRef<mir::Function>>(result);
	}
}
