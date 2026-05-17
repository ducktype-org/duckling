#pragma once

#include "../mir_structure/mir_structure.hpp"

#include <frontend/module_tree/module_id.hpp>
#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::mir {

	struct KeyOf_LowerToMIRFunction {
		// note that HOUTFunction copy is lightweight, cause its uses shared_ptr under the hood
		CRef<helios::HOUTFunction> function;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	using LowerToMIRFunctionResult = query::QResult<Function>;

	/**
	 * @brief Lower a HOUTFunction to a MIRFunction
	 * Performs lifetime analysis.
	 * @note in the future it will validate move semantics and potentially other things.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(LowerToMIRFunction, KeyOf_LowerToMIRFunction, CRef<LowerToMIRFunctionResult>, ({}))

	struct KeyOf_LowerGlobalDataToMIRFunction {
		CRef<helios::HOUTGlobalData> global_data;

		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	using LowerGlobalDataToMIRFunctionResult = query::QResult<Function>;

	/**
	 * @brief Creates a ctor function for a global data.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(
		LowerGlobalDataToMIRCtor,
		KeyOf_LowerGlobalDataToMIRFunction,
		CRef<LowerGlobalDataToMIRFunctionResult>,
		({})
	)
	using ComptimeStatusResult = query::QResult<ComptimeStatus>;

	/**
	 * @brief Entry point for comptime status analysis.
	 */
	DECLARE_QUERY(IsComptimeOnly, helios::SymID, CRef<ComptimeStatusResult>, ({}))

	struct ModuleComptimeMap {
		std::unordered_map<helios::SymID, ComptimeStatus> map;
	};

	using ModuleComptimeMapResult = query::QResult<ModuleComptimeMap>;

	/**
	 * @brief Internal query, to calculate comptime status.
	 */
	DECLARE_QUERY(
		ComptimeStatusCalculate, ::compiler::frontend::ModuleID, CRef<ModuleComptimeMapResult>, ({})
	)
	/**
	 * @brief Lower a HOUTFunction to a "Pre" MIRFunction.
	 * It creates MIR function, but does not perform lifetime analysis and or any checks.
	 * @note Exposed in the interface mostly for tests
	 */
	DECLARE_QUERY(LowerToPreMIRFunction, helios::SymID, CRef<LowerToMIRFunctionResult>, ({}))
}
