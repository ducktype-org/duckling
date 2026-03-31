#include <frontend/pst_parser/pst.hpp>
#include <helios_private/pst_layer/macros.hpp>
#include <query_framework/context/context.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <frontend/pst_parser/elements/hierarchy/statements/expand.hpp>


namespace compiler::helios {

    /**
     * Runs given function for a PST element and all its subelements.
     * Performs recursive calls into macro expansions.
     */
    template<typename ElementT, typename FunctionT>
    void pstForAll(query::Context& ctx, pst::Access<ElementT> element, FunctionT function) {

        if (element->getElementKind() == pst::ElementKind::Expand) {
            auto expansion_result = ctx.query<QueryMacroExpansion>({ 
                element.template dynamicCast<pst::Expand>().value(),
             });
            // PR error handling TODO...
            variant_match(expansion_result.valueOrPanic()) {
                variant_case (pst::AccessLocked<pst::Stmt>, expanded_stmt) {
                    pstForAll(ctx, expanded_stmt.unlock(ctx), function);
                }
                variant_case (ExpansionError<pst::Stmt>, error) {
                    CORE_PANIC("aa");
                }
            }
            return;
        }
        
        // Run the function
        function(element); 

        for (auto child: element->viewChildren()) {
            // handle macros here!
            auto child_unlocked = child.unlockOpt(ctx);
            
            // Note: filterring nullptrs is done on the level of PST children collection,
            // we still we might use out->failed here, when implementing
            // custom logic for most common PST elements (it might improve performance)
            CORE_ASSERT(
                child_unlocked.has_value(),
                "View children should only contain valid element (no null ptrs)"
            );
                
            pstForAll(ctx, child_unlocked.value(), function);
        }

    }


}