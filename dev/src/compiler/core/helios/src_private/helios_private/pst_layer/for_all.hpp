#include <frontend/pst_parser/pst.hpp>


namespace compiler::helios {

    /**
     * Runs given function for a PST element and all its subelements.
     * Performs recursive calls into macro expansions.
     */
    template<typename ElementT, typename FunctionT>
    void pstForAll(query::Context& ctx, pst::Access<ElementT> element, FunctionT function) {
        
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