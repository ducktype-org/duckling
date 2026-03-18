/**
 * @file function_queries.hpp
 * @brief This file contains queries specific to function analysis, return type deduction,
 * and HOUT code generation for functions.
 * @note These queries are primarily used by top-level HELIOS queries during the compilation process.
 */
#pragma once

#include <helios/hout/hout.hpp>
#include <helios/symbols/symbol_id.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

namespace compiler::helios {
	/**
	 * @brief Query declaration of function: types, args and its names.
	 * @note Unlike QueryCodeOfFun, this query works for all SymID-s that represent functions,
	 * be it user-defined, extern, built-in, or generated.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryDeclOfFun, SymID, CRef<query::QResult<HOUTFunctionDeclaration>>, ({}));

	/**
	 * @brief Query code of a function.
	 * @note Works for SymID-s that represent PST-functions (i.e. PST symbol) or generated functions
	 * like default constructors.
	 *
	 * \query_thread_safe_if_cache
	 */
	DECLARE_QUERY(QueryCodeOfFun, SymID, CRef<query::QResult<HOUTFunction>>, ({}));
}
