#pragma once

#include "lookup_result.hpp"

namespace compiler::helios {

    struct AdditionalLookupParameters {
        bool with_wildcards = false;
    };

    /**
     * ...
     */
    class HInterface {

    public:
        /**
         * Performs a lookup in a given interface.
         */
        CRef<LookupResult> lookup(query::Context& ctx, const base::StrID& name, AdditionalLookupParameters = {});


    };


    // some notes:
    // interface should be responsible for performing lookup only,
    // i.e. generating LookupResult.
    // overload resolution should happen in lookup_result probably.
    // lookup result needs some fine tuning.
    //
    // We maybe need a way to figure out if a given symbol is a method of a proper class
    // 

}
