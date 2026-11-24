// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <ctv/ctv.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/value_type.hpp>

#include <query_framework/query_int.hpp>
#include <query_framework/query_result.hpp>

#include <utility>
#include <variant>
#include <vector>

namespace compiler::helios::templates {
	/**
	 * @brief Key for the template argument deduction of a call to a function template.
	 */
	struct TemplateDeductionKey final {
		/**
		 * @brief The function template being called.
		 */
		SymID template_sym_id;

		/**
		 * @brief The types of the positional arguments of the call, in order.
		 */
		std::vector<tsh::ValueType<>> positional_argument_types;

		/**
		 * @brief The names and types of the named arguments of the call, in order.
		 */
		std::vector<std::pair<base::StrID, tsh::ValueType<>>> named_argument_types;

		[[nodiscard]]
		base::Bit256 queryUnstablePerfectHash() const;
	};

	/**
	 * @brief Why the template arguments could not be deduced.
	 */
	struct TemplateDeductionFailure final {
		enum class Reason : uint8_t {
			/**
			 * @brief A template parameter is not a type, which deduction does not support yet.
			 */
			NonTypeParameter,

			/**
			 * @brief No argument determines the template parameter.
			 */
			Undeduced,
		};

		Reason      reason;
		usize       parameter_index;
		base::StrID parameter_name;
	};

	/**
	 * @brief The deduced template arguments, which 1-1 match the template parameters, or the reason
	 * of the failure.
	 */
	using TemplateDeductionResult
		= std::variant<std::vector<ctv::CompileTimeValue>, TemplateDeductionFailure>;

	/**
	 * @brief Deduces the template arguments of a function template from the argument types of a
	 * call.
	 *
	 * The template is baked with a placeholder type for each type parameter, and the parameter
	 * types of the baked declaration are matched against the argument types, from left to right.
	 * A template parameter is determined by the first argument it is matched with; later arguments
	 * are left to the coercions of the call. The deduction is structural: references, pointers,
	 * slices, arrays, tuples, function types and variants are peeled off until a placeholder is
	 * reached, like `T` in `ref T` matched with `ref i64`, or in `T | None` matched with
	 * `i64 | None`.
	 *
	 * @pre The template is a function template (see isFunctionTemplate).
	 * @return The deduced arguments, ready to bake the template, or the reason why they could not
	 * be deduced. Fails only if the template declaration itself fails to compile.
	 */
	DECLARE_QUERY(
		QueryTemplateArgumentDeduction,
		TemplateDeductionKey,
		CRef<query::QResult<TemplateDeductionResult>>,
		({})
	);
}
