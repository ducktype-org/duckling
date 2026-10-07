// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios_private/pst_layer/macros.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/checked_okbad.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/utils/query_failed_try.hpp>

#include <functional>
#include <type_traits>

namespace compiler::helios {

	namespace internal {

		/**
		 * Helper function for pstForAll, that performs the actual recursion.
		 */
		template<typename ElementT, typename FunctionT, typename CutoffFunctionT>
		[[nodiscard]]
		base::OkBad pstForAllAux(
			query::Context&        ctx,
			pst::Access<ElementT>  element,
			const FunctionT&       function,
			const CutoffFunctionT& cutoff_function
		) {
			// Check if we should stop recursion before running the function, to allow cutoff
			// function to skip some branches entirely.
			if (std::invoke(cutoff_function, element)) return base::OK;

			if (element->getElementKind() == pst::ElementKind::Expand) {
				auto expansion_result = ctx.query<QueryMacroExpansion>({
					element.template dynamicCast<pst::Expand>().value(),
				});

				if (expansion_result.hasFailed()) return base::BAD;

				auto inner_result = internal::pstForAllAux(
					ctx, expansion_result.valueOrPanic().unlock(ctx), function, cutoff_function
				);

				return inner_result;
			}

			base::OkBad result = base::OK;

			// Run the function
			function(element);

			for (auto child: element->viewChildren()) {
				auto child_unlocked = child.unlockOpt(ctx);

				// Note: filtering nullptrs is done on the level of PST children collection
				CORE_ASSERT(
					child_unlocked.has_value(),
					"View children should only contain valid element (no null ptrs)"
				);

				auto inner_result
					= internal::pstForAllAux(ctx, child_unlocked.value(), function, cutoff_function);
				if (inner_result.isBad()) result = base::BAD;
			}

			return result;
		}

		/**
		 * Default cutoff function for pstForAll, that never cuts off any branches.
		 * This is needed to make CutoffFunctionT template parameter deduction possible
		 */
		struct CutoffFunctionTDefault final {
			template<typename ElementT>
			bool operator()(pst::Access<ElementT>) const noexcept {
				return false;
			}
		};
	}

	/**
	 * Runs given function for a PST element and all its subelements.
	 * Performs recursive calls into macro expansions.
	 * Skips subtrees for which the cutoff function returns true.
	 *
	 * @return If any query failed during the traversal, returns base::BAD. Otherwise, returns
	 * base::OK.
	 */
	template<
		typename ElementT,
		typename FunctionT,
		typename CutoffFunctionT = internal::CutoffFunctionTDefault>
	requires std::is_invocable_r_v<void, FunctionT, pst::Access<ElementT>>
	      && std::is_invocable_r_v<bool, CutoffFunctionT, pst::Access<ElementT>> [[nodiscard]]
	base::CheckedOkBad pstForAll(
		query::Context&        ctx,
		pst::Access<ElementT>  element,
		const FunctionT&       function,
		const CutoffFunctionT& cutoff_function = internal::CutoffFunctionTDefault{}
	) {
		base::OkBad result = base::OK;

		auto with_failed_exception = query::runFuncWithQueryFailedHandling([&] {
			result = internal::pstForAllAux(ctx, element, function, cutoff_function);
		});
		if (with_failed_exception.status().isBad()) return base::BAD;
		return result;
	}
}
