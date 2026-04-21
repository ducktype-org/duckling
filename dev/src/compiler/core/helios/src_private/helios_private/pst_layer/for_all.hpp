#pragma once

#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <helios_private/pst_layer/macros.hpp>

#include <base/extend_cpp/variant_match.hpp>
#include <base/types/checked_okbad.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/utils/query_failed_try.hpp>

namespace compiler::helios {

	namespace internal {

		/**
		 * Helper function for pstForAll, that performs the actual recursion.
		 */
		template<typename ElementT, typename FunctionT>
		[[nodiscard]]
		base::OkBad pstForAllAux(
			query::Context& ctx, pst::Access<ElementT> element, const FunctionT& function
		) {
			if (element->getElementKind() == pst::ElementKind::Expand) {
				auto expansion_result = ctx.query<QueryMacroExpansion>({
					element.template dynamicCast<pst::Expand>().value(),
				});

				if (expansion_result.hasFailed()) return base::BAD;

				auto inner_result = internal::pstForAllAux(
					ctx, expansion_result.valueOrPanic().unlock(ctx), function
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

				auto inner_result = internal::pstForAllAux(ctx, child_unlocked.value(), function);
				if (inner_result.isBad()) result = base::BAD;
			}

			return result;
		}

	}

	/**
	 * Runs given function for a PST element and all its subelements.
	 * Performs recursive calls into macro expansions.
	 *
	 * @return If any query failed during the traversal, returns base::BAD. Otherwise, returns
	 * base::OK.
	 */
	template<typename ElementT, typename FunctionT>
	[[nodiscard]]
	base::CheckedOkBad pstForAll(
		query::Context& ctx, pst::Access<ElementT> element, const FunctionT& function
	) {
		base::OkBad result = base::OK;

		auto with_failed_exception = query::runFuncWithQueryFailedHandling([&] {
			result = internal::pstForAllAux(ctx, element, function);
		});
		if (with_failed_exception.status().isBad()) return base::BAD;
		return result;
	}

}
