#pragma once

#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>
#include <frontend/pst_parser/pst.hpp>
#include <frontend/pst_parser/elements/hierarchy/meta.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/all_statements.hpp>
#include <frontend/pst_parser/access.hpp>
#include <helios_private/pst_layer/macros.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>

namespace compiler::helios {

	/**
	 * Runs given function for a PST element and all its subelements.
	 * Performs recursive calls into macro expansions.
	 */
	template<typename ElementT, typename FunctionT>
	void pstForAll(query::Context& ctx, pst::Access<ElementT> element, FunctionT function) {
		// std::cerr << "For all: \n";
		// element->debugPrint(std::cerr);
		// std::cerr << "\n";

		if (element->getElementKind() == pst::ElementKind::Expand) {
			auto expansion_result = ctx.query<QueryMacroExpansion>({
				element.template dynamicCast<pst::Expand>().value(),
			});

			// note: valueOrThrow might be suboptimal here.
			variant_match(expansion_result.valueOrThrow()) {
				variant_case(pst::AccessLocked<pst::Stmt>, expanded_stmt) {
					// std::cerr << "\n================\n\n";
					// expanded_stmt.unlock(ctx)->debugPrint(std::cerr);
					pstForAll(ctx, expanded_stmt.unlock(ctx), function);
				}
				variant_case(ExpansionError<pst::Stmt>, error) {
					// @TODO: #2406 deal with this panic
					CORE_PANIC("Error in macro expanded code not handled");
				}
			}
			return;
		}

		// Run the function
		function(element);

		for (auto child: element->viewChildren()) {
			auto child_unlocked = child.unlockOpt(ctx);

			// Note: filtering nullptrs is done on the level of PST children collection
			CORE_ASSERT(
				child_unlocked.has_value(),
				"View children should only contain valid element (no null ptrs)"
			);

			pstForAll(ctx, child_unlocked.value(), function);
		}
	}
}
