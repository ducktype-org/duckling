// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../mir_structure/mir_structure.hpp"

#include <helios/hout/hout.hpp>

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

	struct KeyOf_LowerGlobalData {
		CRef<helios::HOUTGlobalData> global_data;
		[[nodiscard]]
		u64 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Creates a ctor function for a global data.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(LowerGlobalData, KeyOf_LowerGlobalData, CRef<query::QResult<MIRGlobalData>>, ({}))

	/**
	 * @brief Symbols used directly by a function, as collected from its MIR code.
	 * @details `used_functions` holds the HELIOS SymID-s of functions referenced by the code
	 * (e.g. call targets), `used_globals` holds the HELIOS SymID-s of global variables and global
	 * constants that the code reads or writes. Both lists are deduplicated.
	 */
	struct MIRUsedSymbols final {
		std::vector<helios::SymID> used_functions;
		std::vector<helios::SymID> used_globals;
	};

	/**
	 * @brief Compiles the function to MIR code and walks its structure to collect all symbols
	 * (functions and global variables) used directly by that function.
	 *
	 * @note This is a plain helper, not a query. It expects a function SymID that has MIR code,
	 * i.e. one for which `helios::implementsQueryCodeOfFun` returns true; callers are responsible
	 * for that check.
	 */
	MIRUsedSymbols getMIRUsedSymbolsByFunction(query::Context& ctx, helios::SymID function_id);

	/**
	 * @brief Compiles the global data to MIR code and walks its structure to collect all symbols
	 * (functions and global variables) used directly by that global data (ex. by constructor).
	 */
	MIRUsedSymbols getMIRUsedSymbolsByGlobal(query::Context& ctx, helios::SymID global_id);

	/**
	 * @brief Lower a HOUTFunction to a "Pre" MIRFunction.
	 * It creates MIR function, but does not perform lifetime analysis and or any checks.
	 * @note Exposed in the interface mostly for tests
	 */
	Function lowerToPreMIRFunction(query::Context&, CRef<helios::HOUTFunction> function);
}
